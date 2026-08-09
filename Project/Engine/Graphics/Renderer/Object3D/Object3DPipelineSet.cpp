#include "Object3DPipelineSet.h"
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
		enum Object3DRootParameterIndex : UINT
		{
			kMaterialCBV = 0,
			kTransformCBV = 1,
			kBaseTextureSRV = 2,
			kCameraCBV = 3,
			kEnvironmentMapSRV = 4,
			kLightCountCBV = 5,
			kLightArraySRV = 6,
			kDissolveCBV = 7,
			kDissolveMaskSRV = 8,
			kShadowParamCBV = 9,
			kShadowMapSRV = 10,
			kLightingSettingsCBV = 11,
			kMetallicRoughnessSRV = 12,
			kNormalSRV = 13,
			kOcclusionSRV = 14,
			kEmissiveSRV = 15,
			kExtendedShadowCBV = 16,
			kCsmShadowMapSRV = 17,
			kPointShadowMapSRV = 18,
			kCount
		};

		std::array<D3D12_INPUT_ELEMENT_DESC, 3> MakeObject3DInputLayout()
		{
			return {
				D3D12_INPUT_ELEMENT_DESC{
					"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,
					0, D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
				},
				D3D12_INPUT_ELEMENT_DESC{
					"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
					0, D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
				},
				D3D12_INPUT_ELEMENT_DESC{
					"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,
					0, D3D12_APPEND_ALIGNED_ELEMENT,
					D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
				}
			};
		}

		D3D12_ROOT_SIGNATURE_DESC MakeObject3DRootSignatureDesc(
			std::array<D3D12_DESCRIPTOR_RANGE, 11>& ranges,
			std::array<D3D12_ROOT_PARAMETER, Object3DRootParameterIndex::kCount>& rootParameters,
			std::array<D3D12_STATIC_SAMPLER_DESC, 3>& staticSamplers)
		{
			ranges[0] = {};
			ranges[0].BaseShaderRegister = 0;
			ranges[0].NumDescriptors = 1;
			ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[1] = {};
			ranges[1].BaseShaderRegister = 1;
			ranges[1].NumDescriptors = 1;
			ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[2] = {};
			ranges[2].BaseShaderRegister = 2;
			ranges[2].NumDescriptors = 1;
			ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[3] = {};
			ranges[3].BaseShaderRegister = 3;
			ranges[3].NumDescriptors = 1;
			ranges[3].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[3].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[4] = {};
			ranges[4].BaseShaderRegister = 4;
			ranges[4].NumDescriptors = 1;
			ranges[4].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			ranges[4].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			for (size_t i = 5; i < 9; ++i)
			{
				ranges[i] = {};
				ranges[i].BaseShaderRegister = static_cast<UINT>(i + 1); // t6～t9を既存Registerの後ろへ追加する。
				ranges[i].NumDescriptors = 1;
				ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}
			for (size_t i = 9; i < ranges.size(); ++i)
			{
				ranges[i] = {};
				ranges[i].BaseShaderRegister = static_cast<UINT>(i + 1); // t10:CSM t11:PointCube
				ranges[i].NumDescriptors = 1;
				ranges[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			}

			rootParameters[kMaterialCBV] = {};
			rootParameters[kMaterialCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kMaterialCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kMaterialCBV].Descriptor.ShaderRegister = 0;

			rootParameters[kTransformCBV] = {};
			rootParameters[kTransformCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kTransformCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[kTransformCBV].Descriptor.ShaderRegister = 0;

			rootParameters[kBaseTextureSRV] = {};
			rootParameters[kBaseTextureSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kBaseTextureSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kBaseTextureSRV].DescriptorTable.pDescriptorRanges = &ranges[0];
			rootParameters[kBaseTextureSRV].DescriptorTable.NumDescriptorRanges = 1;

			rootParameters[kCameraCBV] = {};
			rootParameters[kCameraCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kCameraCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kCameraCBV].Descriptor.ShaderRegister = 1;

			rootParameters[kEnvironmentMapSRV] = {};
			rootParameters[kEnvironmentMapSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kEnvironmentMapSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kEnvironmentMapSRV].DescriptorTable.pDescriptorRanges = &ranges[1];
			rootParameters[kEnvironmentMapSRV].DescriptorTable.NumDescriptorRanges = 1;

			rootParameters[kLightCountCBV] = {};
			rootParameters[kLightCountCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kLightCountCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kLightCountCBV].Descriptor.ShaderRegister = 2;

			rootParameters[kLightArraySRV] = {};
			rootParameters[kLightArraySRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kLightArraySRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kLightArraySRV].DescriptorTable.pDescriptorRanges = &ranges[2];
			rootParameters[kLightArraySRV].DescriptorTable.NumDescriptorRanges = 1;

			rootParameters[kDissolveCBV] = {};
			rootParameters[kDissolveCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kDissolveCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kDissolveCBV].Descriptor.ShaderRegister = 3;

			rootParameters[kDissolveMaskSRV] = {};
			rootParameters[kDissolveMaskSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kDissolveMaskSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kDissolveMaskSRV].DescriptorTable.pDescriptorRanges = &ranges[3];
			rootParameters[kDissolveMaskSRV].DescriptorTable.NumDescriptorRanges = 1;

			rootParameters[kShadowParamCBV] = {};
			rootParameters[kShadowParamCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kShadowParamCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			rootParameters[kShadowParamCBV].Descriptor.ShaderRegister = 4;

			rootParameters[kShadowMapSRV] = {};
			rootParameters[kShadowMapSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kShadowMapSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kShadowMapSRV].DescriptorTable.pDescriptorRanges = &ranges[4];
			rootParameters[kShadowMapSRV].DescriptorTable.NumDescriptorRanges = 1;

			rootParameters[kLightingSettingsCBV] = {};
			rootParameters[kLightingSettingsCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kLightingSettingsCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kLightingSettingsCBV].Descriptor.ShaderRegister = 5;

			rootParameters[kMetallicRoughnessSRV] = {};
			rootParameters[kMetallicRoughnessSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kMetallicRoughnessSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kMetallicRoughnessSRV].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[kMetallicRoughnessSRV].DescriptorTable.pDescriptorRanges = &ranges[5];

			rootParameters[kNormalSRV] = {};
			rootParameters[kNormalSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kNormalSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kNormalSRV].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[kNormalSRV].DescriptorTable.pDescriptorRanges = &ranges[6];

			rootParameters[kOcclusionSRV] = {};
			rootParameters[kOcclusionSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kOcclusionSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kOcclusionSRV].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[kOcclusionSRV].DescriptorTable.pDescriptorRanges = &ranges[7];

			rootParameters[kEmissiveSRV] = {};
			rootParameters[kEmissiveSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kEmissiveSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kEmissiveSRV].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[kEmissiveSRV].DescriptorTable.pDescriptorRanges = &ranges[8];

			rootParameters[kExtendedShadowCBV] = {};
			rootParameters[kExtendedShadowCBV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[kExtendedShadowCBV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kExtendedShadowCBV].Descriptor.ShaderRegister = 6;

			rootParameters[kCsmShadowMapSRV] = {};
			rootParameters[kCsmShadowMapSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kCsmShadowMapSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kCsmShadowMapSRV].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[kCsmShadowMapSRV].DescriptorTable.pDescriptorRanges = &ranges[9];

			rootParameters[kPointShadowMapSRV] = {};
			rootParameters[kPointShadowMapSRV].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[kPointShadowMapSRV].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[kPointShadowMapSRV].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[kPointShadowMapSRV].DescriptorTable.pDescriptorRanges = &ranges[10];

			staticSamplers[0] = {};
			staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
			staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			staticSamplers[0].ShaderRegister = 0;
			staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;

			staticSamplers[1] = {};
			staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
			staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			staticSamplers[1].ShaderRegister = 1;
			staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;

			staticSamplers[2] = staticSamplers[0];
			staticSamplers[2].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			staticSamplers[2].ShaderRegister = 2;

			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(rootParameters.size());
			desc.pParameters = rootParameters.data();
			desc.NumStaticSamplers = static_cast<UINT>(staticSamplers.size());
			desc.pStaticSamplers = staticSamplers.data();
			return desc;
		}

		D3D12_ROOT_SIGNATURE_DESC MakeShadowRootSignatureDesc(std::array<D3D12_ROOT_PARAMETER, 1>& rootParameters)
		{
			rootParameters[0] = {};
			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[0].Descriptor.ShaderRegister = 0;

			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(rootParameters.size());
			desc.pParameters = rootParameters.data();
			desc.NumStaticSamplers = 0;
			desc.pStaticSamplers = nullptr;
			return desc;
		}

		GraphicsPipelineDesc MakeBaseObject3DDesc(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, const D3D12_INPUT_LAYOUT_DESC& inputLayout)
		{
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
			return desc;
		}

		GraphicsPipelineDesc MakeBaseShadowDesc(const D3D12_INPUT_LAYOUT_DESC& inputLayout)
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

	void Object3DPipelineSet::Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		assert(dxcManager != nullptr);

		auto inputElements = MakeObject3DInputLayout();

		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		inputLayout.pInputElementDescs = inputElements.data();
		inputLayout.NumElements = static_cast<UINT>(inputElements.size());

		const ShaderDescriptor& objectVs = Object3DShaderManifest::Get(Object3DShaderId::Object3DVS);
		const ShaderDescriptor& objectPs = Object3DShaderManifest::Get(Object3DShaderId::Object3DPS);
		const ShaderDescriptor& shadowVs = Object3DShaderManifest::Get(Object3DShaderId::ShadowMapVS);

		assert(objectVs.stage == ShaderStage::Vertex);
		assert(objectPs.stage == ShaderStage::Pixel);
		assert(shadowVs.stage == ShaderStage::Vertex);

		ComPtr<IDxcBlob> objectVsBlob = ShaderCompiler::CompileShader(objectVs, dxcManager);
		ComPtr<IDxcBlob> objectPsBlob = ShaderCompiler::CompileShader(objectPs, dxcManager);
		ComPtr<IDxcBlob> shadowVsBlob = ShaderCompiler::CompileShader(shadowVs, dxcManager);

		{
			std::array<D3D12_DESCRIPTOR_RANGE, 11> ranges{};
			std::array<D3D12_ROOT_PARAMETER, Object3DRootParameterIndex::kCount> rootParameters{};
			std::array<D3D12_STATIC_SAMPLER_DESC, 3> staticSamplers{};

			D3D12_ROOT_SIGNATURE_DESC rootSigDesc = MakeObject3DRootSignatureDesc(ranges, rootParameters, staticSamplers);

			GraphicsPipelineDesc desc = MakeBaseObject3DDesc(rtvFormat, dsvFormat, inputLayout);
			desc.debugName = L"Object3D.Default";
			desc.shaders.vertexShader.blob = objectVsBlob;
			desc.shaders.pixelShader.blob = objectPsBlob;

			defaultPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

			if (defaultPipeline_.rootSignature) defaultPipeline_.rootSignature->SetName(L"Object3D.Default.RootSignature");
			if (defaultPipeline_.pipelineState) defaultPipeline_.pipelineState->SetName(L"Object3D.Default.PSO");
		}

		{
			std::array<D3D12_DESCRIPTOR_RANGE, 11> ranges{};
			std::array<D3D12_ROOT_PARAMETER, Object3DRootParameterIndex::kCount> rootParameters{};
			std::array<D3D12_STATIC_SAMPLER_DESC, 3> staticSamplers{};

			D3D12_ROOT_SIGNATURE_DESC rootSigDesc = MakeObject3DRootSignatureDesc(ranges, rootParameters, staticSamplers);
			GraphicsPipelineDesc desc = MakeBaseObject3DDesc(rtvFormat, dsvFormat, inputLayout);
			desc.blendState = PipelineStatePresets::MakeBlendAlpha();
			desc.depthStencilState = PipelineStatePresets::MakeDepthReadOnly();
			desc.debugName = L"Object3D.Alpha";
			desc.shaders.vertexShader.blob = objectVsBlob;
			desc.shaders.pixelShader.blob = objectPsBlob;

			alphaPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc); // 残像はDepthを読みつつ書き込まず、後続の本体描画を妨げない。

			if (alphaPipeline_.rootSignature) alphaPipeline_.rootSignature->SetName(L"Object3D.Alpha.RootSignature");
			if (alphaPipeline_.pipelineState) alphaPipeline_.pipelineState->SetName(L"Object3D.Alpha.PSO");
		}

		{
			std::array<D3D12_ROOT_PARAMETER, 1> rootParameters{};
			D3D12_ROOT_SIGNATURE_DESC rootSigDesc = MakeShadowRootSignatureDesc(rootParameters);

			GraphicsPipelineDesc desc = MakeBaseShadowDesc(inputLayout);
			desc.debugName = L"Object3D.Shadow";
			desc.shaders.vertexShader.blob = shadowVsBlob;

			shadowPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

			if (shadowPipeline_.rootSignature) shadowPipeline_.rootSignature->SetName(L"Object3D.Shadow.RootSignature");
			if (shadowPipeline_.pipelineState) shadowPipeline_.pipelineState->SetName(L"Object3D.Shadow.PSO");
		}
	}

	void Object3DPipelineSet::Finalize()
	{
		defaultPipeline_.Reset();
		alphaPipeline_.Reset();
		shadowPipeline_.Reset();
	}
} // namespace Ken4lowEngine
