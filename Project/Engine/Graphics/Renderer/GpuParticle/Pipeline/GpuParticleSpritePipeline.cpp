#include "GpuParticleSpritePipeline.h"
#include <DirectXCommon.h>
#include <LogString.h>

#include "BlendStateFactory.h"
#include "ShaderCompiler.h"
#include "GpuParticleShaderManifest.h"

#include <string>

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void GpuParticleSpritePipeline::Initialize()
{
	dxCommon_ = DirectXCommon::GetInstance();

	CreateRootSignature();
	for (uint32_t modeIndex = 0; modeIndex < static_cast<uint32_t>(BlendMode::kcountOfBlendMode); ++modeIndex)
	{
		CreatePSO(static_cast<BlendMode>(modeIndex));
	}

	rootSignature_->SetName(L"GpuParticleSpritePipeline_RootSignature");
}

ID3D12PipelineState* GpuParticleSpritePipeline::GetGfxPSO(BlendMode blendMode) const
{
	const size_t index = static_cast<size_t>(blendMode);
	if (index < pipelineStates_.size() && pipelineStates_[index])
	{
		return pipelineStates_[index].Get();
	}

	return pipelineStates_[static_cast<size_t>(BlendMode::kBlendModeAdd)].Get();
}

/// -------------------------------------------------------------
/// Graphics PSO 生成（GPUスプライト用）
/// -------------------------------------------------------------
void GpuParticleSpritePipeline::CreatePSO(BlendMode blendMode)
{
	HRESULT hr{};

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,  0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,        0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElementDescs[2] = { "NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT,     0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// AuthoringのBlendModeごとにPSOを分離し、同一シェーダーでも合成方法だけを安全に切り替える。
	const D3D12_RENDER_TARGET_BLEND_DESC blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode);

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	const ShaderDescriptor& vsDesc =
		GpuParticleShaderManifest::GetGraphics(GpuParticleGraphicsShaderId::SpriteVS);
	const ShaderDescriptor& psDesc =
		GpuParticleShaderManifest::GetGraphics(GpuParticleGraphicsShaderId::SpritePS);

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

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	depthStencilDesc.DepthEnable = TRUE;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSignature_.Get();
	desc.InputLayout = inputLayoutDesc;
	desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
	desc.BlendState.RenderTarget[0] = blendDesc;
	desc.RasterizerState = rasterizerDesc;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.DepthStencilState = depthStencilDesc;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	const size_t index = static_cast<size_t>(blendMode);
	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineStates_[index]));
	assert(SUCCEEDED(hr));

	const std::wstring name = L"GpuParticleSpritePipeline_Gfx_PSO_" + std::to_wstring(index);
	pipelineStates_[index]->SetName(name.c_str());
}

/// -------------------------------------------------------------
/// RootSignature 生成（GPUスプライト用）
/// -------------------------------------------------------------
void GpuParticleSpritePipeline::CreateRootSignature()
{
	HRESULT hr{};

	D3D12_ROOT_SIGNATURE_DESC rsDesc{};
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE srvRange[1]{};
	srvRange[0].BaseShaderRegister = 0;
	srvRange[0].NumDescriptors = 1;
	srvRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	srvRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER params[4]{};

	params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	params[0].Descriptor.ShaderRegister = 0;

	params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	params[1].DescriptorTable.NumDescriptorRanges = _countof(srvRange);
	params[1].DescriptorTable.pDescriptorRanges = srvRange;

	params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // VSでもrenderGroup/dead判定を行い、不要粒子をラスタライズ前に除外する。
	params[2].Descriptor.ShaderRegister = 1;

	params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	params[3].DescriptorTable.NumDescriptorRanges = _countof(srvRange);
	params[3].DescriptorTable.pDescriptorRanges = srvRange;

	rsDesc.pParameters = params;
	rsDesc.NumParameters = _countof(params);

	D3D12_STATIC_SAMPLER_DESC samplers[1]{};
	samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
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

} // namespace Ken4lowEngine
