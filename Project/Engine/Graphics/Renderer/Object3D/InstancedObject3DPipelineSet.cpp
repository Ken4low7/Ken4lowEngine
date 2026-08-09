#include "InstancedObject3DPipelineSet.h"
#include "PipelineFactory.h"
#include "PipelineStatePresets.h"
#include "ShaderCompiler.h"
#include "Object3DShaderManifest.h"
#include "DXCCompilerManager.h"

#include <array>
#include <cassert>

namespace Ken4lowEngine
{
	namespace
	{
		enum RootParameterIndex : UINT
		{
			kMaterialCBV,
			kPerViewCBV,
			kBaseTextureSRV,
			kCameraCBV,
			kEnvironmentMapSRV,
			kLightCountCBV,
			kLightArraySRV,
			kDissolveCBV,
			kDissolveMaskSRV,
			kShadowParamCBV,
			kShadowMapSRV,
			kLightingSettingsCBV,
			kInstanceDataSRV,
			kMetallicRoughnessSRV,
			kNormalSRV,
			kOcclusionSRV,
			kEmissiveSRV,
			kExtendedShadowCBV,
			kCsmShadowMapSRV,
			kPointShadowMapSRV,
			kCount
		};

		std::array<D3D12_INPUT_ELEMENT_DESC, 3> MakeInputLayout()
		{
			return {
				D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};
		}

		D3D12_ROOT_SIGNATURE_DESC MakeRootSignatureDesc(
			std::array<D3D12_DESCRIPTOR_RANGE, 12>& ranges,
			std::array<D3D12_ROOT_PARAMETER, kCount>& parameters,
			std::array<D3D12_STATIC_SAMPLER_DESC, 3>& samplers)
		{
			const UINT registers[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
			for (size_t i = 0; i < ranges.size(); ++i)
			{
				ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				ranges[i].NumDescriptors = 1;
				ranges[i].BaseShaderRegister = registers[i];
				ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}

			auto setCBV = [&](UINT index, UINT shaderRegister, D3D12_SHADER_VISIBILITY visibility)
			{
				parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
				parameters[index].ShaderVisibility = visibility;
				parameters[index].Descriptor.ShaderRegister = shaderRegister;
			};
			auto setTable = [&](UINT index, size_t rangeIndex, D3D12_SHADER_VISIBILITY visibility)
			{
				parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				parameters[index].ShaderVisibility = visibility;
				parameters[index].DescriptorTable.NumDescriptorRanges = 1;
				parameters[index].DescriptorTable.pDescriptorRanges = &ranges[rangeIndex];
			};

			setCBV(kMaterialCBV, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kPerViewCBV, 0, D3D12_SHADER_VISIBILITY_VERTEX);
			setTable(kBaseTextureSRV, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kCameraCBV, 1, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kEnvironmentMapSRV, 1, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kLightCountCBV, 2, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kLightArraySRV, 2, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kDissolveCBV, 3, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kDissolveMaskSRV, 3, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kShadowParamCBV, 4, D3D12_SHADER_VISIBILITY_ALL);
			setTable(kShadowMapSRV, 4, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kLightingSettingsCBV, 5, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kInstanceDataSRV, 5, D3D12_SHADER_VISIBILITY_VERTEX);
			setTable(kMetallicRoughnessSRV, 6, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kNormalSRV, 7, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kOcclusionSRV, 8, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kEmissiveSRV, 9, D3D12_SHADER_VISIBILITY_PIXEL);
			setCBV(kExtendedShadowCBV, 6, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kCsmShadowMapSRV, 10, D3D12_SHADER_VISIBILITY_PIXEL);
			setTable(kPointShadowMapSRV, 11, D3D12_SHADER_VISIBILITY_PIXEL);

			samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			samplers[0].ShaderRegister = 0;
			samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			samplers[0].MaxLOD = D3D12_FLOAT32_MAX;

			samplers[1] = samplers[0];
			samplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			samplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			samplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			samplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			samplers[1].ShaderRegister = 1;

			samplers[2] = samplers[0];
			samplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			samplers[2].ShaderRegister = 2;

			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(parameters.size());
			desc.pParameters = parameters.data();
			desc.NumStaticSamplers = static_cast<UINT>(samplers.size());
			desc.pStaticSamplers = samplers.data();
			return desc;
		}

		D3D12_ROOT_SIGNATURE_DESC MakeShadowRootSignatureDesc(
			D3D12_DESCRIPTOR_RANGE& instanceRange,
			std::array<D3D12_ROOT_PARAMETER, 2>& parameters)
		{
			instanceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			instanceRange.NumDescriptors = 1;
			instanceRange.BaseShaderRegister = 0;
			instanceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			parameters[0].Descriptor.ShaderRegister = 0;

			parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			parameters[1].DescriptorTable.NumDescriptorRanges = 1;
			parameters[1].DescriptorTable.pDescriptorRanges = &instanceRange;

			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(parameters.size());
			desc.pParameters = parameters.data();
			return desc;
		}

		GraphicsPipelineDesc MakeShadowPipelineDesc(const D3D12_INPUT_LAYOUT_DESC& inputLayout)
		{
			GraphicsPipelineDesc desc{};
			desc.blendState = PipelineStatePresets::MakeBlendOpaque();

			auto rasterizer = PipelineStatePresets::MakeRasterizerCullBack();
			rasterizer.DepthBias = 300;
			rasterizer.SlopeScaledDepthBias = 0.75f;
			rasterizer.DepthBiasClamp = 0.0f;
			desc.rasterizerState = rasterizer;

			desc.depthStencilState = PipelineStatePresets::MakeDepthReadWrite();
			desc.numRenderTargets = 0;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			desc.sampleCount = 1;
			desc.inputLayout = inputLayout;
			return desc;
		}
	}

	void InstancedObject3DPipelineSet::Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		assert(dxcManager != nullptr);
		auto elements = MakeInputLayout();
		D3D12_INPUT_LAYOUT_DESC inputLayout{ elements.data(), static_cast<UINT>(elements.size()) };

		const ShaderDescriptor& vs = Object3DShaderManifest::Get(Object3DShaderId::Object3DInstancingVS);
		const ShaderDescriptor& ps = Object3DShaderManifest::Get(Object3DShaderId::Object3DPS);
		const ShaderDescriptor& shadowVs = Object3DShaderManifest::Get(Object3DShaderId::Object3DInstancingShadowVS);
		ComPtr<IDxcBlob> vsBlob = ShaderCompiler::CompileShader(vs, dxcManager);
		ComPtr<IDxcBlob> psBlob = ShaderCompiler::CompileShader(ps, dxcManager);
		ComPtr<IDxcBlob> shadowVsBlob = ShaderCompiler::CompileShader(shadowVs, dxcManager);

		{
			std::array<D3D12_DESCRIPTOR_RANGE, 12> ranges{};
			std::array<D3D12_ROOT_PARAMETER, kCount> parameters{};
			std::array<D3D12_STATIC_SAMPLER_DESC, 3> samplers{};
			D3D12_ROOT_SIGNATURE_DESC rootDesc = MakeRootSignatureDesc(ranges, parameters, samplers);

			GraphicsPipelineDesc desc{};
			desc.blendState = PipelineStatePresets::MakeBlendOpaque();
			desc.rasterizerState = PipelineStatePresets::MakeRasterizerCullNone();
			desc.depthStencilState = PipelineStatePresets::MakeDepthReadWrite();
			desc.numRenderTargets = 1;
			desc.rtvFormats[0] = rtvFormat;
			desc.dsvFormat = dsvFormat;
			desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			desc.sampleCount = 1;
			desc.inputLayout = inputLayout;
			desc.debugName = L"Object3D.Instanced";
			desc.shaders.vertexShader.blob = vsBlob;
			desc.shaders.pixelShader.blob = psBlob;

			defaultPipeline_ = factory.CreateGraphicsPipeline(desc, rootDesc);
			if (defaultPipeline_.rootSignature) { defaultPipeline_.rootSignature->SetName(L"Object3D.Instanced.RootSignature"); }
			if (defaultPipeline_.pipelineState) { defaultPipeline_.pipelineState->SetName(L"Object3D.Instanced.PSO"); }
		}

		{
			D3D12_DESCRIPTOR_RANGE instanceRange{};
			std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
			D3D12_ROOT_SIGNATURE_DESC rootDesc = MakeShadowRootSignatureDesc(instanceRange, parameters);

			GraphicsPipelineDesc desc = MakeShadowPipelineDesc(inputLayout);
			desc.debugName = L"Object3D.InstancedShadow";
			desc.shaders.vertexShader.blob = shadowVsBlob;

			shadowPipeline_ = factory.CreateGraphicsPipeline(desc, rootDesc);
			if (shadowPipeline_.rootSignature) { shadowPipeline_.rootSignature->SetName(L"Object3D.InstancedShadow.RootSignature"); }
			if (shadowPipeline_.pipelineState) { shadowPipeline_.pipelineState->SetName(L"Object3D.InstancedShadow.PSO"); }
		}
	}

	void InstancedObject3DPipelineSet::Finalize()
	{
		shadowPipeline_.Reset();
		defaultPipeline_.Reset();
	}
}
