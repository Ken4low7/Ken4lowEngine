#include "PostEffectPipelineBuilder.h"
#include "DirectXCommon.h"
#include "ShaderCompiler.h"

#include <cassert>
#include <vector>

namespace Ken4lowEngine
{

	using namespace Microsoft::WRL;

	/// -------------------------------------------------------------
	///                         初期化処理
	/// -------------------------------------------------------------
	void PostEffectPipelineBuilder::Initialize(DirectXCommon* dxCommon)
	{
		assert(dxCommon != nullptr);
		dxCommon_ = dxCommon;
	}

	void PostEffectPipelineBuilder::Finalize()
	{
		copyPipelineState_.Reset();
		copyRootSignature_.Reset();
		dxCommon_ = nullptr;
	}

	/// -------------------------------------------------------------
	///            PostEffect Graphics 用契約の検証
	/// -------------------------------------------------------------
	void PostEffectPipelineBuilder::ValidatePostEffectGraphicsContracts(const ShaderDescriptor& vsDesc, const ShaderDescriptor& psDesc, ID3D12RootSignature* rootSignature)
	{
		// ルートシグネチャは null であってはならない
		if (rootSignature == nullptr)
		{
			assert(false && "RootSignature must not be null for PostEffect graphics pipeline");
		}

		// PostEffect Graphics Pipeline では、頂点シェーダーは Vertex ステージ、ピクセルシェーダーは Pixel ステージである必要がある
		if (vsDesc.stage != ShaderStage::Vertex || psDesc.stage != ShaderStage::Pixel)
		{
			assert(false && "Invalid shader stages for PostEffect graphics pipeline");
		}

		// PostEffect Graphics Pipeline では、両方のシェーダーが PostEffect 用ルートシグネチャに関連付けられている必要がある
		if (vsDesc.rootSignature != RootSignatureType::PostEffect || psDesc.rootSignature != RootSignatureType::PostEffect)
		{
			assert(false && "Shaders must be associated with PostEffect root signature for PostEffect graphics pipeline");
		}
	}

	/// -------------------------------------------------------------
	///            PostEffect Compute 用契約の検証
	/// -------------------------------------------------------------
	void PostEffectPipelineBuilder::ValidatePostEffectComputeContracts(const ShaderDescriptor& csDesc, ID3D12RootSignature* rootSignature)
	{
		// ルートシグネチャは null であってはならない
		if (rootSignature == nullptr)
		{
			assert(false && "RootSignature must not be null for PostEffect compute pipeline");
		}

		// PostEffect Compute Pipeline では、コンピュートシェーダーは Compute ステージである必要がある
		if (csDesc.stage != ShaderStage::Compute)
		{
			assert(false && "Invalid shader stage for PostEffect compute pipeline");
		}

		// PostEffect Compute Pipeline では、コンピュートシェーダーは Compute 用ルートシグネチャに関連付けられている必要がある
		if (csDesc.rootSignature != RootSignatureType::Compute)
		{
			assert(false && "Shader must be associated with Compute root signature for PostEffect compute pipeline");
		}
	}

	/// -------------------------------------------------------------
	///                 ルートシグネチャの生成
	/// -------------------------------------------------------------
	ComPtr<ID3D12RootSignature> PostEffectPipelineBuilder::CreateRootSignature()
	{
		assert(dxCommon_ != nullptr);

		D3D12_STATIC_SAMPLER_DESC samplers[2] = {};

		// s0: バイリニア
		samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplers[0].ShaderRegister = 0;
		samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// s1: ポイント
		samplers[1] = samplers[0];
		samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplers[1].ShaderRegister = 1;

		std::vector<D3D12_DESCRIPTOR_RANGE> descriptorRanges;

		for (int i = 0; i <= 2; ++i)
		{
			D3D12_DESCRIPTOR_RANGE range{};
			range.BaseShaderRegister = static_cast<UINT>(i);
			range.NumDescriptors = 1;
			range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			descriptorRanges.push_back(range);
		}

		std::vector<D3D12_ROOT_PARAMETER> rootParams;

		// t0 : gTexture
		D3D12_ROOT_PARAMETER texParam{};
		texParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		texParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
		texParam.DescriptorTable.NumDescriptorRanges = 1;
		texParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(texParam);

		// b0, b1
		for (int i = 0; i < 2; ++i)
		{
			D3D12_ROOT_PARAMETER cbv{};
			cbv.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			cbv.Descriptor.ShaderRegister = static_cast<UINT>(i);
			cbv.Descriptor.RegisterSpace = 0;
			cbv.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParams.push_back(cbv);
		}

		// t1 : gMask
		D3D12_ROOT_PARAMETER maskParam{};
		maskParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		maskParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[1];
		maskParam.DescriptorTable.NumDescriptorRanges = 1;
		maskParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(maskParam);

		// t2 : gDepth
		D3D12_ROOT_PARAMETER depthParam{};
		depthParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		depthParam.DescriptorTable.pDescriptorRanges = &descriptorRanges[2];
		depthParam.DescriptorTable.NumDescriptorRanges = 1;
		depthParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParams.push_back(depthParam);

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		rootSigDesc.pParameters = rootParams.data();
		rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
		rootSigDesc.pStaticSamplers = samplers;
		rootSigDesc.NumStaticSamplers = _countof(samplers);

		ComPtr<ID3DBlob> sigBlob;
		ComPtr<ID3DBlob> errBlob;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&sigBlob,
			&errBlob);
		assert(SUCCEEDED(hr) && "RootSignature Serialize Failed");

		ComPtr<ID3D12RootSignature> rootSig;
		hr = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			sigBlob->GetBufferPointer(),
			sigBlob->GetBufferSize(),
			IID_PPV_ARGS(&rootSig));
		assert(SUCCEEDED(hr) && "CreateRootSignature Failed");

		return rootSig;
	}

	/// -------------------------------------------------------------
	///             グラフィックスパイプラインの生成
	/// -------------------------------------------------------------
	ComPtr<ID3D12PipelineState> PostEffectPipelineBuilder::CreateGraphicsPipeline(PostEffectGraphicsShaderId pixelShaderId, ID3D12RootSignature* rootSignature, bool enableDepth)
	{
		assert(dxCommon_ != nullptr);
		assert(rootSignature != nullptr);

		const ShaderDescriptor& vsDesc =
			PostEffectShaderManifest::GetGraphics(PostEffectGraphicsShaderId::FullscreenVS);
		const ShaderDescriptor& psDesc =
			PostEffectShaderManifest::GetGraphics(pixelShaderId);

		ValidatePostEffectGraphicsContracts(vsDesc, psDesc, rootSignature);

		auto vs = ShaderCompiler::CompileShader(
			vsDesc,
			dxCommon_->GetDXCCompilerManager());

		auto ps = ShaderCompiler::CompileShader(
			psDesc,
			dxCommon_->GetDXCCompilerManager());

		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature;
		desc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
		desc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleDesc.Count = 1;
		desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

		// フルスクリーン描画前提
		desc.InputLayout = { nullptr, 0 };

		D3D12_RASTERIZER_DESC rasterizerDesc{};
		rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;
		rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterizerDesc.DepthClipEnable = TRUE;
		rasterizerDesc.MultisampleEnable = FALSE;
		rasterizerDesc.AntialiasedLineEnable = FALSE;
		rasterizerDesc.ForcedSampleCount = 0;
		rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
		desc.RasterizerState = rasterizerDesc;

		D3D12_BLEND_DESC blendDesc{};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		auto& rt = blendDesc.RenderTarget[0];
		rt.BlendEnable = FALSE;
		rt.LogicOpEnable = FALSE;
		rt.SrcBlend = D3D12_BLEND_ONE;
		rt.DestBlend = D3D12_BLEND_ZERO;
		rt.BlendOp = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_ZERO;
		rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.LogicOp = D3D12_LOGIC_OP_NOOP;
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		desc.BlendState = blendDesc;

		if (enableDepth)
		{
			D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
			depthStencilDesc.DepthEnable = TRUE;
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			depthStencilDesc.StencilEnable = FALSE;
			desc.DepthStencilState = depthStencilDesc;
			desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
		}
		else
		{
			D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
			depthStencilDesc.DepthEnable = FALSE;
			depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
			depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
			depthStencilDesc.StencilEnable = FALSE;
			desc.DepthStencilState = depthStencilDesc;
			desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
		}

		ComPtr<ID3D12PipelineState> pso;
		HRESULT hr;
		hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pso));
		assert(SUCCEEDED(hr));

		return pso;
	}

	/// -------------------------------------------------------------
	///             コンピュートルートシグネチャの生成
	/// -------------------------------------------------------------
	ComPtr<ID3D12RootSignature> PostEffectPipelineBuilder::CreateComputeRootSignature()
	{
		assert(dxCommon_ != nullptr);

		D3D12_DESCRIPTOR_RANGE srvRange0{};
		srvRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange0.NumDescriptors = 1;
		srvRange0.BaseShaderRegister = 0;
		srvRange0.RegisterSpace = 0;
		srvRange0.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE srvRange1{};
		srvRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange1.NumDescriptors = 1;
		srvRange1.BaseShaderRegister = 1;
		srvRange1.RegisterSpace = 0;
		srvRange1.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE uavRange{};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0;
		uavRange.RegisterSpace = 0;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		std::vector<D3D12_ROOT_PARAMETER> rootParams(4);

		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &srvRange0;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &uavRange;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[2].Descriptor.ShaderRegister = 0;
		rootParams[2].Descriptor.RegisterSpace = 0;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[3].DescriptorTable.pDescriptorRanges = &srvRange1;
		rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC samplerDesc[2]{};
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc[0].ShaderRegister = 0;
		samplerDesc[0].RegisterSpace = 0;
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		samplerDesc[1] = samplerDesc[0];
		samplerDesc[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc[1].ShaderRegister = 1;

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
		rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
		rootSigDesc.pParameters = rootParams.data();
		rootSigDesc.NumStaticSamplers = _countof(samplerDesc);
		rootSigDesc.pStaticSamplers = samplerDesc;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		ComPtr<ID3DBlob> sigBlob;
		ComPtr<ID3DBlob> errorBlob;
		HRESULT hr = D3D12SerializeRootSignature(
			&rootSigDesc,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&sigBlob,
			&errorBlob);
		assert(SUCCEEDED(hr) && "RootSignature Serialize Failed");

		ComPtr<ID3D12RootSignature> computeRootSignature;
		hr = dxCommon_->GetDevice()->CreateRootSignature(
			0,
			sigBlob->GetBufferPointer(),
			sigBlob->GetBufferSize(),
			IID_PPV_ARGS(&computeRootSignature));
		assert(SUCCEEDED(hr) && "CreateComputeRootSignature Failed");

		return computeRootSignature;
	}

	/// -------------------------------------------------------------
	///             コンピュートパイプラインの生成
	/// -------------------------------------------------------------
	ComPtr<ID3D12PipelineState> PostEffectPipelineBuilder::CreateComputePipeline(
		PostEffectComputeShaderId computeShaderId,
		ID3D12RootSignature* rootSignature)
	{
		assert(dxCommon_ != nullptr);
		assert(rootSignature != nullptr);

		const ShaderDescriptor& csDesc =
			PostEffectShaderManifest::GetCompute(computeShaderId);

		ValidatePostEffectComputeContracts(csDesc, rootSignature);

		auto cs = ShaderCompiler::CompileShader(
			csDesc,
			dxCommon_->GetDXCCompilerManager());
		assert(cs != nullptr);

		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = rootSignature;
		desc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

		ComPtr<ID3D12PipelineState> pso;
		HRESULT hr{};
		hr = dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pso));
		assert(SUCCEEDED(hr) && "CreateComputePipelineState Failed");

		return pso;
	}

	/// -------------------------------------------------------------
	///                 コピー用パイプラインの構築
	/// -------------------------------------------------------------
	void PostEffectPipelineBuilder::BuildCopyPipeline()
	{
		copyRootSignature_ = CreateRootSignature();
		copyPipelineState_ = CreateGraphicsPipeline(
			PostEffectGraphicsShaderId::NormalPS,
			copyRootSignature_.Get(),
			false);

		copyPipelineState_->SetName(L"PostEffectPipelineBuilder_Copy_PSO");
		copyRootSignature_->SetName(L"PostEffectPipelineBuilder_Copy_RootSignature");
	}

} // namespace Ken4lowEngine