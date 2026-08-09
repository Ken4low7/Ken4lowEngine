#define NOMINMAX
#include "BlockEffectRenderer.h"

#include "BlendModeType.h"
#include "BlendStateFactory.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "PipelineStatePresets.h"
#include "ResourceManager.h"
#include "ShaderCompiler.h"
#include "SRVManager.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <utility>

namespace
{
	constexpr UINT kRootViewProjection = 0;
	constexpr UINT kRootInstances = 1;
}

BlockEffectRenderer::BlockEffectRenderer(std::wstring debugName)
	: debugName_(std::move(debugName))
{
}

BlockEffectRenderer::~BlockEffectRenderer()
{
	ReleaseSrv();
}

void BlockEffectRenderer::Draw(const std::vector<BlockInstance>& instances)
{
	if (instances.empty()) { return; }
	if (!initialized_) { Initialize(); }

	EnsureInstanceCapacity(instances.size());
	std::copy(instances.begin(), instances.end(), instanceData_);
	viewProjectionData_->viewProjection = K4E::CameraManager::GetInstance()->GetActiveViewProjectionMatrix();

	auto* commandList = K4E::DirectXCommon::GetInstance()->GetCommandManager()->GetCommandList();
	K4E::SRVManager::GetInstance()->PreDraw();
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(kRootViewProjection, viewProjectionBuffer_->GetGPUVirtualAddress());
	K4E::SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(kRootInstances, srvIndex_);
	commandList->DrawIndexedInstanced(indexCount_, static_cast<UINT>(instances.size()), 0, 0, 0);
}

void BlockEffectRenderer::Initialize()
{
	CreatePipeline();
	CreateCubeMesh();

	auto* device = K4E::DirectXCommon::GetInstance()->GetDevice();
	viewProjectionBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(ViewProjectionData));
	viewProjectionBuffer_->SetName((debugName_ + L"_ViewProjectionBuffer").c_str());
	viewProjectionBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&viewProjectionData_));
	EnsureInstanceCapacity(1);
	initialized_ = true;
}

void BlockEffectRenderer::CreatePipeline()
{
	auto* dxCommon = K4E::DirectXCommon::GetInstance();
	auto* device = dxCommon->GetDevice();

	std::array<D3D12_DESCRIPTOR_RANGE, 1> ranges{};
	ranges[0].BaseShaderRegister = 0;
	ranges[0].NumDescriptors = 1;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	std::array<D3D12_ROOT_PARAMETER, 2> rootParameters{};
	rootParameters[kRootViewProjection].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[kRootViewProjection].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[kRootViewProjection].Descriptor.ShaderRegister = 0;
	rootParameters[kRootInstances].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[kRootInstances].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	rootParameters[kRootInstances].DescriptorTable.pDescriptorRanges = ranges.data();
	rootParameters[kRootInstances].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.NumParameters = static_cast<UINT>(rootParameters.size());
	rootSignatureDesc.pParameters = rootParameters.data();

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	assert(SUCCEEDED(hr));
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	const K4E::ShaderDescriptor vsDesc{ L"BlockEffectVS", L"Resources/Shaders/Disintegration/DisintegrationBlock.VS.hlsl", L"main", L"vs_6_0", K4E::ShaderStage::Vertex, K4E::RootSignatureType::Object3D };
	const K4E::ShaderDescriptor psDesc{ L"BlockEffectPS", L"Resources/Shaders/Disintegration/DisintegrationBlock.PS.hlsl", L"main", L"ps_6_0", K4E::ShaderStage::Pixel, K4E::RootSignatureType::Object3D };
	auto vertexShader = K4E::ShaderCompiler::CompileShader(vsDesc, dxCommon->GetDXCCompilerManager());
	auto pixelShader = K4E::ShaderCompiler::CompileShader(psDesc, dxCommon->GetDXCCompilerManager());

	D3D12_INPUT_ELEMENT_DESC inputElement{};
	inputElement.SemanticName = "POSITION";
	inputElement.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElement.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElement.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { &inputElement, 1 };
	psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
	psoDesc.BlendState.RenderTarget[0] = K4E::BlendStateFactory::GetInstance()->GetBlendDesc(K4E::BlendMode::kBlendModeNormal);
	psoDesc.RasterizerState = K4E::PipelineStatePresets::MakeRasterizerCullNone();
	psoDesc.DepthStencilState = K4E::PipelineStatePresets::MakeDepthReadOnly();
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void BlockEffectRenderer::CreateCubeMesh()
{
	static const std::array<CubeVertex, 8> vertices{
		CubeVertex{ { -0.5f, -0.5f, -0.5f } }, CubeVertex{ { -0.5f,  0.5f, -0.5f } },
		CubeVertex{ {  0.5f,  0.5f, -0.5f } }, CubeVertex{ {  0.5f, -0.5f, -0.5f } },
		CubeVertex{ { -0.5f, -0.5f,  0.5f } }, CubeVertex{ { -0.5f,  0.5f,  0.5f } },
		CubeVertex{ {  0.5f,  0.5f,  0.5f } }, CubeVertex{ {  0.5f, -0.5f,  0.5f } },
	};
	static const std::array<uint32_t, 36> indices{
		0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0,
		3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7,
	};

	auto* device = K4E::DirectXCommon::GetInstance()->GetDevice();
	vertexBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(CubeVertex) * vertices.size());
	vertexBuffer_->SetName((debugName_ + L"_CubeVertexBuffer").c_str());
	void* vertexMapped = nullptr;
	vertexBuffer_->Map(0, nullptr, &vertexMapped);
	std::memcpy(vertexMapped, vertices.data(), sizeof(CubeVertex) * vertices.size());
	vertexBufferView_ = { vertexBuffer_->GetGPUVirtualAddress(), static_cast<UINT>(sizeof(CubeVertex) * vertices.size()), sizeof(CubeVertex) };

	indexBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(uint32_t) * indices.size());
	indexBuffer_->SetName((debugName_ + L"_CubeIndexBuffer").c_str());
	void* indexMapped = nullptr;
	indexBuffer_->Map(0, nullptr, &indexMapped);
	std::memcpy(indexMapped, indices.data(), sizeof(uint32_t) * indices.size());
	indexBufferView_ = { indexBuffer_->GetGPUVirtualAddress(), static_cast<UINT>(sizeof(uint32_t) * indices.size()), DXGI_FORMAT_R32_UINT };
	indexCount_ = static_cast<UINT>(indices.size());
}

void BlockEffectRenderer::EnsureInstanceCapacity(size_t requiredCapacity)
{
	if (requiredCapacity <= instanceCapacity_) { return; }

	if (instanceBuffer_) { retiredInstanceBuffers_.push_back(instanceBuffer_); }
	if (srvIndex_ != UINT32_MAX)
	{
		retiredSrvIndices_.push_back(srvIndex_);
		srvIndex_ = UINT32_MAX;
	}

	instanceCapacity_ = std::max(requiredCapacity, instanceCapacity_ * 2 + 1);
	auto* device = K4E::DirectXCommon::GetInstance()->GetDevice();
	instanceBuffer_ = K4E::ResourceManager::CreateBufferResource(device, sizeof(BlockInstance) * instanceCapacity_);
	instanceBuffer_->SetName((debugName_ + L"_InstanceBuffer").c_str());
	instanceBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&instanceData_));
	srvIndex_ = K4E::SRVManager::GetInstance()->Allocate();
	K4E::SRVManager::GetInstance()->CreateSRVForStructureBuffer(srvIndex_, instanceBuffer_.Get(), static_cast<UINT>(instanceCapacity_), sizeof(BlockInstance));
}

void BlockEffectRenderer::ReleaseSrv()
{
	if (srvIndex_ != UINT32_MAX)
	{
		K4E::SRVManager::GetInstance()->Free(srvIndex_);
		srvIndex_ = UINT32_MAX;
	}
	for (const uint32_t retiredSrvIndex : retiredSrvIndices_)
	{
		K4E::SRVManager::GetInstance()->Free(retiredSrvIndex);
	}
	retiredSrvIndices_.clear();
	retiredInstanceBuffers_.clear();
	instanceData_ = nullptr;
	instanceBuffer_.Reset();
	instanceCapacity_ = 0;
}
