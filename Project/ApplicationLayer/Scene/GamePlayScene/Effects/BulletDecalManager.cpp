#define NOMINMAX
#include "BulletDecalManager.h"

#include "BlendStateFactory.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "ShaderCompiler.h"
#include "ShaderManifestTypes.h"
#include "SRVManager.h"
#include "TextureManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <numbers>

namespace
{
	constexpr float kNormalEpsilon = 1.0e-5f;
	constexpr float kSurfaceOffset = 0.01f;
	constexpr float kDefaultLifeTime = 30.0f;

	float LengthSquared(const K4E::Vector3& value)
	{
		return value.x * value.x + value.y * value.y + value.z * value.z;
	}
}

BulletDecalManager::~BulletDecalManager()
{
	Finalize();
}

bool BulletDecalManager::Initialize()
{
	Finalize();
	dxCommon_ = K4E::DirectXCommon::GetInstance();
	if (!dxCommon_)
	{
		return false;
	}

	texturePath_ = ResolveTexturePath();
	K4E::TextureManager::GetInstance()->LoadTexture(texturePath_);
	initialized_ = CreatePipeline() && CreateBuffers();
	return initialized_;
}

void BulletDecalManager::Finalize()
{
	// 銃痕共有Quad・index・インスタンス・ViewProjectionの永続Mapを解除する。
	if (vertexBuffer_) vertexBuffer_->Unmap(0, nullptr);
	if (indexBuffer_) indexBuffer_->Unmap(0, nullptr);
	if (instanceBuffer_) instanceBuffer_->Unmap(0, nullptr);
	if (viewProjectionBuffer_) viewProjectionBuffer_->Unmap(0, nullptr);
	mappedVertices_ = nullptr;
	mappedIndices_ = nullptr;
	mappedInstances_ = nullptr;
	mappedViewProjection_ = nullptr;
	vertexBuffer_.Reset();
	indexBuffer_.Reset();
	instanceBuffer_.Reset();
	viewProjectionBuffer_.Reset();
	pipelineState_.Reset();
	rootSignature_.Reset();
	decals_.clear();
	texturePath_.clear();
	dxCommon_ = nullptr;
	initialized_ = false;
}

void BulletDecalManager::AddDecal(const K4E::Vector3& hitPosition, const K4E::Vector3& hitNormal)
{
	const bool hasFiniteNormal = std::isfinite(hitNormal.x) && std::isfinite(hitNormal.y) && std::isfinite(hitNormal.z);
	const bool hasFinitePosition = std::isfinite(hitPosition.x) && std::isfinite(hitPosition.y) && std::isfinite(hitPosition.z);
	if (!enabled_ || !initialized_ || !hasFiniteNormal || !hasFinitePosition ||
		LengthSquared(hitNormal) <= kNormalEpsilon * kNormalEpsilon)
	{
		return;
	}

	const K4E::Vector3 normal = K4E::Vector3::Normalize(hitNormal);
	BulletDecal decal{};
	// 着弾位置から少し法線方向へ浮かせ、壁面とのZ-fightingを防ぐ。
	decal.position = hitPosition + normal * kSurfaceOffset;
	decal.normal = normal;
	decal.scale = { 0.18f, 0.18f, 1.0f };
	decal.lifeTime = kDefaultLifeTime;
	std::uniform_real_distribution<float> rotationDistribution(0.0f, std::numbers::pi_v<float> * 2.0f);
	decal.rotation = rotationDistribution(random_);
	decal.world = MakeDecalWorldMatrix(decal);

	// 銃痕が無限に増えて重くならないよう、上限到達時は最古の1件を捨てる。
	if (decals_.size() >= kMaxBulletDecals)
	{
		decals_.pop_front();
	}
	decals_.push_back(decal);
}

void BulletDecalManager::Update(float deltaTime)
{
	if (deltaTime <= 0.0f)
	{
		return;
	}

	for (BulletDecal& decal : decals_)
	{
		decal.lifeTime -= deltaTime;
	}
	while (!decals_.empty() && decals_.front().lifeTime <= 0.0f)
	{
		decals_.pop_front();
	}
	decals_.erase(std::remove_if(decals_.begin(), decals_.end(), [](const BulletDecal& decal)
		{
			return decal.lifeTime <= 0.0f;
		}), decals_.end());
}

void BulletDecalManager::Draw()
{
	if (!enabled_ || !initialized_ || decals_.empty())
	{
		return;
	}

	mappedViewProjection_->viewProjection = K4E::CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
	for (size_t i = 0; i < decals_.size(); ++i)
	{
		mappedInstances_[i].world = decals_[i].world;
		mappedInstances_[i].color = decals_[i].color;
	}

	auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
	K4E::SRVManager::GetInstance()->PreDraw();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const D3D12_VERTEX_BUFFER_VIEW views[] = { vertexBufferView_, instanceBufferView_ };
	commandList->IASetVertexBuffers(0, _countof(views), views);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, viewProjectionBuffer_->GetGPUVirtualAddress());
	K4E::TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(
		commandList, 1, K4E::TextureManager::GetInstance()->GetSrvHandleGPU(texturePath_));
	// 銃痕用の板ポリを壁面方向に向け、共有Quadを全銃痕分まとめて描画する。
	commandList->DrawIndexedInstanced(6, static_cast<UINT>(decals_.size()), 0, 0, 0);
}

void BulletDecalManager::DrawImGui()
{
#ifdef USE_IMGUI
	if (ImGui::CollapsingHeader("Bullet Decals"))
	{
		ImGui::Text("Bullet Decal Count: %zu", decals_.size());
		ImGui::Text("Max Bullet Decals: %zu", kMaxBulletDecals);
		ImGui::Checkbox("Enable Bullet Decals", &enabled_);
		if (ImGui::Button("Clear Bullet Decals"))
		{
			Clear();
		}
	}
#endif
}

void BulletDecalManager::Clear()
{
	decals_.clear();
}

K4E::Matrix4x4 BulletDecalManager::MakeDecalWorldMatrix(const BulletDecal& decal) const
{
	const K4E::Vector3 forward = decal.normal;
	const K4E::Vector3 reference = std::fabs(forward.y) < 0.999f
		? K4E::Vector3(0.0f, 1.0f, 0.0f)
		: K4E::Vector3(1.0f, 0.0f, 0.0f);
	const K4E::Vector3 baseRight = K4E::Vector3::Normalize(K4E::Vector3::Cross(reference, forward));
	const K4E::Vector3 baseUp = K4E::Vector3::Normalize(K4E::Vector3::Cross(forward, baseRight));
	const float cosine = std::cos(decal.rotation);
	const float sine = std::sin(decal.rotation);
	const K4E::Vector3 right = baseRight * cosine + baseUp * sine;
	const K4E::Vector3 up = baseUp * cosine - baseRight * sine;

	// Quadのローカル+Zを面法線へ合わせ、面内回転と小さいスケールを行ベクトル規約で組む。
	K4E::Matrix4x4 world = K4E::Matrix4x4::MakeIdentity();
	world.m[0][0] = right.x * decal.scale.x;
	world.m[0][1] = right.y * decal.scale.x;
	world.m[0][2] = right.z * decal.scale.x;
	world.m[1][0] = up.x * decal.scale.y;
	world.m[1][1] = up.y * decal.scale.y;
	world.m[1][2] = up.z * decal.scale.y;
	world.m[2][0] = forward.x;
	world.m[2][1] = forward.y;
	world.m[2][2] = forward.z;
	world.m[3][0] = decal.position.x;
	world.m[3][1] = decal.position.y;
	world.m[3][2] = decal.position.z;
	return world;
}

bool BulletDecalManager::CreateBuffers()
{
	const std::array<VertexData, 4> vertices = {
		VertexData{ { -0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f } },
		VertexData{ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f } },
		VertexData{ {  0.5f,  0.5f, 0.0f }, { 1.0f, 0.0f } },
		VertexData{ {  0.5f, -0.5f, 0.0f }, { 1.0f, 1.0f } },
	};
	constexpr std::array<uint32_t, 6> indices = { 0, 1, 2, 0, 2, 3 };

	vertexBuffer_ = K4E::ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(vertices));
	indexBuffer_ = K4E::ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(indices));
	instanceBuffer_ = K4E::ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(InstanceData) * kMaxBulletDecals);
	viewProjectionBuffer_ = K4E::ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ViewProjectionData));
	if (!vertexBuffer_ || !indexBuffer_ || !instanceBuffer_ || !viewProjectionBuffer_) return false;

	// 銃痕共有Quad、index、インスタンス、ViewProjectionバッファを永続Mapする。
	vertexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedVertices_));
	indexBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedIndices_));
	instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstances_));
	viewProjectionBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedViewProjection_));
	std::copy(vertices.begin(), vertices.end(), mappedVertices_);
	std::copy(indices.begin(), indices.end(), mappedIndices_);
	vertexBufferView_ = { vertexBuffer_->GetGPUVirtualAddress(), sizeof(vertices), sizeof(VertexData) };
	indexBufferView_ = { indexBuffer_->GetGPUVirtualAddress(), sizeof(indices), DXGI_FORMAT_R32_UINT };
	instanceBufferView_ = { instanceBuffer_->GetGPUVirtualAddress(), sizeof(InstanceData) * kMaxBulletDecals, sizeof(InstanceData) };
	return true;
}

bool BulletDecalManager::CreatePipeline()
{
	D3D12_DESCRIPTOR_RANGE textureRange{};
	textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	textureRange.NumDescriptors = 1;
	textureRange.BaseShaderRegister = 0;

	D3D12_ROOT_PARAMETER rootParameters[2]{};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].DescriptorTable.pDescriptorRanges = &textureRange;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rootDesc{};
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootDesc.NumParameters = _countof(rootParameters);
	rootDesc.pParameters = rootParameters;
	rootDesc.NumStaticSamplers = 1;
	rootDesc.pStaticSamplers = &sampler;
	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	if (FAILED(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob))) return false;
	if (FAILED(dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(rootSignature_.GetAddressOf())))) return false;

	const K4E::ShaderDescriptor vsDesc{ L"BulletDecalVS", L"Resources/Shaders/BulletDecal/BulletDecal.VS.hlsl", L"main", L"vs_6_0", K4E::ShaderStage::Vertex, K4E::RootSignatureType::Object3D };
	const K4E::ShaderDescriptor psDesc{ L"BulletDecalPS", L"Resources/Shaders/BulletDecal/BulletDecal.PS.hlsl", L"main", L"ps_6_0", K4E::ShaderStage::Pixel, K4E::RootSignatureType::Object3D };
	auto vs = K4E::ShaderCompiler::CompileShader(vsDesc, dxCommon_->GetDXCCompilerManager());
	auto ps = K4E::ShaderCompiler::CompileShader(psDesc, dxCommon_->GetDXCCompilerManager());
	if (!vs || !ps) return false;

	const D3D12_INPUT_ELEMENT_DESC elements[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
	};
	D3D12_RASTERIZER_DESC rasterizer{};
	rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer.CullMode = D3D12_CULL_MODE_NONE;
	D3D12_DEPTH_STENCIL_DESC depth{};
	depth.DepthEnable = TRUE;
	depth.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depth.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = rootSignature_.Get();
	pso.InputLayout = { elements, _countof(elements) };
	pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	pso.BlendState.RenderTarget[0] = K4E::BlendStateFactory::GetInstance()->GetBlendDesc(K4E::BlendMode::kBlendModeNormal);
	pso.RasterizerState = rasterizer;
	pso.DepthStencilState = depth;
	pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pso.SampleDesc.Count = 1;
	return SUCCEEDED(dxCommon_->GetDevice()->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(pipelineState_.GetAddressOf())));
}

std::string BulletDecalManager::ResolveTexturePath() const
{
	if (std::filesystem::exists("Resources/Textures/Compiled/Effects/BulletHole.dds")) return "Effects/BulletHole.dds";
	if (std::filesystem::exists("Resources/Textures/Compiled/Effects/circle2.dds")) return "Effects/circle2.dds";
	// 専用テクスチャも円形仮テクスチャも無い場合は、必ず存在する白テクスチャへフォールバックする。
	return "Effects/white.dds";
}
