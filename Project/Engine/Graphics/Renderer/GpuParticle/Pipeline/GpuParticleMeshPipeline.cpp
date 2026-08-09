#include "GpuParticleMeshPipeline.h"
#include <DirectXCommon.h>
#include <LogString.h>

#include "BlendStateFactory.h"
#include "ShaderCompiler.h"
#include "GpuParticleShaderManifest.h"

namespace Ken4lowEngine
{

void GpuParticleMeshPipeline::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();

	CreateRootSignature();

	CreatePSO();

	pipelineState_->SetName(L"GpuParticleMeshPipeline_Gfx_PSO");

	rootSignature_->SetName(L"GpuParticleMeshPipeline_RootSignature");
}

void GpuParticleMeshPipeline::CreateRootSignature()
{
	HRESULT hr{};

	D3D12_ROOT_SIGNATURE_DESC rsDesc{};
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// SRV Table（t0 を 1つ）
	D3D12_DESCRIPTOR_RANGE srvRange[1]{};
	srvRange[0].BaseShaderRegister = 0;
	srvRange[0].NumDescriptors = 1;
	srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// RootParameters（スプライトと同じ構成で開始）
	D3D12_ROOT_PARAMETER params[4]{};

	// b0 : PerView / 定数（全部のシェーダ）
	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	params[0].Descriptor.ShaderRegister = 0;

	// t0 : Particle SRV（VS）
	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].DescriptorTable.NumDescriptorRanges = _countof(srvRange);
	params[1].DescriptorTable.pDescriptorRanges = srvRange;

	// b1 : Material（PS）
	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[2].Descriptor.ShaderRegister = 1;

	// t0 : Texture SRV（PS）
	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[3].DescriptorTable.NumDescriptorRanges = _countof(srvRange);
	params[3].DescriptorTable.pDescriptorRanges = srvRange;

	rsDesc.pParameters = params;
	rsDesc.NumParameters = _countof(params);

	// Static Sampler（スプライトと同じで開始）
	D3D12_STATIC_SAMPLER_DESC samplers[1]{};
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	samplers[0].ShaderRegister = 0;
	samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rsDesc.pStaticSamplers = samplers;
	rsDesc.NumStaticSamplers = _countof(samplers);

	ComPtr<ID3DBlob> sig;
	ComPtr<ID3DBlob> err;
	hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
	if (FAILED(hr))
	{
		Log(err ? reinterpret_cast<char*>(err->GetBufferPointer()) : "SerializeRootSignature failed");
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));
}

void GpuParticleMeshPipeline::CreatePSO()
{
	HRESULT hr{};

	// ---- 入力レイアウト（まずはスプライトと同じにしておく）----
	// ※あなたのモデル頂点が float3 なら POSITION の Format を R32G32B32_FLOAT に変更してね
	D3D12_INPUT_ELEMENT_DESC input[3]{};
	input[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	input[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,	   D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	input[2] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0,	   D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = input;
	layout.NumElements = _countof(input);

	// ---- Blend ----
	const D3D12_RENDER_TARGET_BLEND_DESC blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);

	// ---- Rasterizer（メッシュなので基本BACK。粒子用途で両面欲しければNONE）----
	D3D12_RASTERIZER_DESC rast{};
	rast.CullMode = D3D12_CULL_MODE_BACK;
	rast.FillMode = D3D12_FILL_MODE_SOLID;

	// ---- Depth（推奨：テストON・書き込みOFF）----
	D3D12_DEPTH_STENCIL_DESC ds{};
	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	const ShaderDescriptor& vsDesc =
		GpuParticleShaderManifest::GetGraphics(GpuParticleGraphicsShaderId::MeshVS);
	const ShaderDescriptor& psDesc =
		GpuParticleShaderManifest::GetGraphics(GpuParticleGraphicsShaderId::MeshPS);

	assert(vsDesc.stage == ShaderStage::Vertex);
	assert(psDesc.stage == ShaderStage::Pixel);
	assert(vsDesc.rootSignature == RootSignatureType::GpuParticle);
	assert(psDesc.rootSignature == RootSignatureType::GpuParticle);

	ComPtr<IDxcBlob> vs = ShaderCompiler::CompileShader(
		vsDesc,
		dxCommon_->GetDXCCompilerManager());
	assert(vs != nullptr);

	ComPtr<IDxcBlob> ps = ShaderCompiler::CompileShader(
		psDesc,
		dxCommon_->GetDXCCompilerManager());
	assert(ps != nullptr);

	// ---- PSO ----
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.InputLayout = layout;
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.BlendState.RenderTarget[0] = blendDesc;
	desc.RasterizerState = rast;

	desc.DepthStencilState = ds;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

} // namespace Ken4lowEngine
