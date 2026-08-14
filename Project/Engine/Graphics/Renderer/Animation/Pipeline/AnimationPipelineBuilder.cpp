#include "AnimationPipelineBuilder.h"
#include "DirectXCommon.h"
#include "LightManager.h"
#include "AnimationShaderManifest.h"
#include "PipelineStatePresets.h"
#include <LogString.h>
#include <ShaderCompiler.h>
#include <BlendStateFactory.h>

namespace Ken4lowEngine
{
	namespace
	{
		D3D12_RASTERIZER_DESC MakeAnimationRasterizer(MaterialCullMode cullMode)
		{
			switch (cullMode)
			{
			case MaterialCullMode::Front: return PipelineStatePresets::MakeRasterizerCullFront();
			case MaterialCullMode::None: return PipelineStatePresets::MakeRasterizerCullNone();
			case MaterialCullMode::Back:
			default: return PipelineStatePresets::MakeRasterizerCullBack();
			}
		}
	}

	AnimationPipelineBuilder* AnimationPipelineBuilder::GetInstance()
	{
		static AnimationPipelineBuilder instance;
		return &instance;
	}

	void AnimationPipelineBuilder::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		CreateRootSignature();
		CreatePSO();
		CreateComputeRootSignature();
		CreateComputePSO();
		LightManager::GetInstance()->Initialize(dxCommon_);

		rootSignature->SetName(L"AnimationPipelineBuilder::rootSignature");
		graphicsPipelineState->SetName(L"AnimationPipelineBuilder::graphicsPipelineState.Back");
		graphicsPipelineStateFront_->SetName(L"AnimationPipelineBuilder::graphicsPipelineState.Front");
		graphicsPipelineStateTwoSided_->SetName(L"AnimationPipelineBuilder::graphicsPipelineState.TwoSided");
		computeRootSignature_->SetName(L"AnimationPipelineBuilder::computeRootSignature");
		computePipelineState_->SetName(L"AnimationPipelineBuilder::computePipelineState");
	}

	void AnimationPipelineBuilder::Finalize()
	{
		LightManager::GetInstance()->Finalize();
		computePipelineState_.Reset();
		computeRootSignature_.Reset();
		graphicsPipelineStateTwoSided_.Reset();
		graphicsPipelineStateFront_.Reset();
		graphicsPipelineState.Reset();
		rootSignature.Reset();
		dxCommon_ = nullptr;
		blendMode_ = BlendMode::kBlendModeNone;
	}

	ID3D12PipelineState* AnimationPipelineBuilder::GetPipelineState(MaterialCullMode cullMode) const
	{
		switch (cullMode)
		{
		case MaterialCullMode::Front: return graphicsPipelineStateFront_.Get();
		case MaterialCullMode::None: return graphicsPipelineStateTwoSided_.Get();
		case MaterialCullMode::Back:
		default: return graphicsPipelineState.Get();
		}
	}

	void AnimationPipelineBuilder::SetRenderSetting(MaterialCullMode cullMode)
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->SetGraphicsRootSignature(rootSignature.Get());
		commandList->SetPipelineState(GetPipelineState(cullMode)); // Skinned BatchもMaterial Cull ModeごとのPSOを共有する。
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(9);
		LightManager::GetInstance()->BindExtendedShadowResources(14, 15, 16);
	}

	void AnimationPipelineBuilder::SetComputeSetting()
	{
		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->SetComputeRootSignature(computeRootSignature_.Get());
		commandList->SetPipelineState(computePipelineState_.Get());
	}

	void AnimationPipelineBuilder::CreateRootSignature()
	{
		D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
		descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
		staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
		staticSamplers[0].ShaderRegister = 0;
		staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		staticSamplers[1] = {};
		staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
		staticSamplers[1].ShaderRegister = 1;
		staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		descriptionRootSignature.pStaticSamplers = staticSamplers;
		descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

		D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
		descriptorRange[0].BaseShaderRegister = 0;
		descriptorRange[0].NumDescriptors = 1;
		descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE cubeMapRange{};
		cubeMapRange.BaseShaderRegister = 1;
		cubeMapRange.NumDescriptors = 1;
		cubeMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		cubeMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE lightArrayRange{};
		lightArrayRange.BaseShaderRegister = 2;
		lightArrayRange.NumDescriptors = 1;
		lightArrayRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		lightArrayRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE shadowMapRange{};
		shadowMapRange.BaseShaderRegister = 4;
		shadowMapRange.NumDescriptors = 1;
		shadowMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		shadowMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE materialTextureRanges[4]{};
		for (UINT i = 0; i < _countof(materialTextureRanges); ++i)
		{
			materialTextureRanges[i].BaseShaderRegister = 6 + i; // t6:MR t7:Normal t8:AO t9:Emissive
			materialTextureRanges[i].NumDescriptors = 1;
			materialTextureRanges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			materialTextureRanges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		}

		D3D12_DESCRIPTOR_RANGE csmShadowMapRange{};
		csmShadowMapRange.BaseShaderRegister = 10;
		csmShadowMapRange.NumDescriptors = 1;
		csmShadowMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		csmShadowMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_DESCRIPTOR_RANGE pointShadowMapRange{};
		pointShadowMapRange.BaseShaderRegister = 11;
		pointShadowMapRange.NumDescriptors = 1;
		pointShadowMapRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		pointShadowMapRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		D3D12_ROOT_PARAMETER rootParameters[17] = {};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		rootParameters[1].Descriptor.ShaderRegister = 0;
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
		rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[3].Descriptor.ShaderRegister = 1;
		rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[4].DescriptorTable.pDescriptorRanges = &cubeMapRange;
		rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[5].Descriptor.ShaderRegister = 2;
		rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[6].DescriptorTable.pDescriptorRanges = &lightArrayRange;
		rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParameters[7].Descriptor.ShaderRegister = 4;
		rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[8].DescriptorTable.pDescriptorRanges = &shadowMapRange;
		rootParameters[8].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[9].Descriptor.ShaderRegister = 5; // Skinning PSもObject3Dと同じLightingSettingsを使う。
		for (UINT i = 0; i < _countof(materialTextureRanges); ++i)
		{
			rootParameters[10 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[10 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[10 + i].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[10 + i].DescriptorTable.pDescriptorRanges = &materialTextureRanges[i];
		}
		rootParameters[14].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[14].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[14].Descriptor.ShaderRegister = 6;
		rootParameters[15].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[15].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[15].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[15].DescriptorTable.pDescriptorRanges = &csmShadowMapRange;
		rootParameters[16].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[16].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		rootParameters[16].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[16].DescriptorTable.pDescriptorRanges = &pointShadowMapRange;
		descriptionRootSignature.pParameters = rootParameters;
		descriptionRootSignature.NumParameters = _countof(rootParameters);

		ComPtr<ID3DBlob> signatureBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
		if (FAILED(hr))
		{
			Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
			assert(false);
		}
		hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
		assert(SUCCEEDED(hr));
	}

	void AnimationPipelineBuilder::CreatePSO()
	{
		std::array<D3D12_INPUT_ELEMENT_DESC, 5> inputElementDescs{};
		inputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[2] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[3] = { "WEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		inputElementDescs[4] = { "INDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
		D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{ inputElementDescs.data(), static_cast<UINT>(inputElementDescs.size()) };

		const D3D12_RENDER_TARGET_BLEND_DESC blendDesc = BlendStateFactory::GetInstance()->GetBlendDesc(blendMode_);
		const ShaderDescriptor& vsDesc = AnimationShaderManifest::GetGraphics(AnimationGraphicsShaderId::SkinningObject3DVS);
		const ShaderDescriptor& psDesc = AnimationShaderManifest::GetGraphics(AnimationGraphicsShaderId::SkinningObject3DPS);
		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		assert(vsDesc.rootSignature == RootSignatureType::Skinned);
		assert(psDesc.rootSignature == RootSignatureType::Skinned);
		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(vsDesc, dxCommon_->GetDXCCompilerManager());
		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(psDesc, dxCommon_->GetDXCCompilerManager());
		assert(vertexShaderBlob != nullptr && pixelShaderBlob != nullptr);

		D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
		depthStencilDesc.DepthEnable = true;
		depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC baseDesc{};
		baseDesc.pRootSignature = rootSignature.Get();
		baseDesc.InputLayout = inputLayoutDesc;
		baseDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
		baseDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
		baseDesc.BlendState.RenderTarget[0] = blendDesc;
		baseDesc.NumRenderTargets = 1;
		baseDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		baseDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		baseDesc.SampleDesc.Count = 1;
		baseDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		baseDesc.DepthStencilState = depthStencilDesc;
		baseDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		auto createPipeline = [&](MaterialCullMode cullMode, ComPtr<ID3D12PipelineState>& destination)
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = baseDesc;
			desc.RasterizerState = MakeAnimationRasterizer(cullMode); // Shaderは共有し、Rasterizer stateだけMaterial Cull Modeごとに分ける。
			const HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&destination));
			assert(SUCCEEDED(hr));
		};
		createPipeline(MaterialCullMode::Back, graphicsPipelineState);
		createPipeline(MaterialCullMode::Front, graphicsPipelineStateFront_);
		createPipeline(MaterialCullMode::None, graphicsPipelineStateTwoSided_);
	}

	void AnimationPipelineBuilder::CreateComputeRootSignature()
	{
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
		D3D12_DESCRIPTOR_RANGE srvRange2{};
		srvRange2.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		srvRange2.NumDescriptors = 1;
		srvRange2.BaseShaderRegister = 2;
		srvRange2.RegisterSpace = 0;
		srvRange2.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		D3D12_DESCRIPTOR_RANGE uavRange{};
		uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		uavRange.NumDescriptors = 1;
		uavRange.BaseShaderRegister = 0;
		uavRange.RegisterSpace = 0;
		uavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		std::vector<D3D12_ROOT_PARAMETER> rootParams(6);
		rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[0].DescriptorTable.pDescriptorRanges = &srvRange0;
		rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange1;
		rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[2].DescriptorTable.pDescriptorRanges = &srvRange2;
		rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParams[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParams[3].DescriptorTable.pDescriptorRanges = &uavRange;
		rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[4].Descriptor.ShaderRegister = 0;
		rootParams[4].Descriptor.RegisterSpace = 0;
		rootParams[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		rootParams[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParams[5].Descriptor.ShaderRegister = 1;
		rootParams[5].Descriptor.RegisterSpace = 0;
		rootParams[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
		samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc[0].ShaderRegister = 0;
		samplerDesc[0].RegisterSpace = 0;
		samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
		rootSigDesc.NumParameters = static_cast<UINT>(rootParams.size());
		rootSigDesc.pParameters = rootParams.data();
		rootSigDesc.NumStaticSamplers = 1;
		rootSigDesc.pStaticSamplers = samplerDesc;
		rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
		ComPtr<ID3DBlob> sigBlob, errorBlog;
		HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlog);
		assert(SUCCEEDED(hr) && "RootSignature Serialize Failed");
		hr = dxCommon_->GetDevice()->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&computeRootSignature_));
		assert(SUCCEEDED(hr) && "CreateRootSignature Failed");
	}

	void AnimationPipelineBuilder::CreateComputePSO()
	{
		const ShaderDescriptor& csDesc = AnimationShaderManifest::GetCompute(AnimationComputeShaderId::SkinningObject3DCS);
		assert(csDesc.stage == ShaderStage::Compute);
		assert(csDesc.rootSignature == RootSignatureType::Compute);
		ComPtr<IDxcBlob> computeShader = ShaderCompiler::CompileShader(csDesc, dxCommon_->GetDXCCompilerManager());
		assert(computeShader != nullptr);
		D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = computeRootSignature_.Get();
		desc.CS = { computeShader->GetBufferPointer(), computeShader->GetBufferSize() };
		HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&computePipelineState_));
		assert(SUCCEEDED(hr) && "CreateComputePipelineState Failed");
	}

} // namespace Ken4lowEngine
