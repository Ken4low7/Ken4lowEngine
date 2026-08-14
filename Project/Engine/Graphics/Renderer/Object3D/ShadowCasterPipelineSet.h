#pragma once

#include "DX12Include.h"
#include "DXCCompilerManager.h"
#include "Object3DShaderManifest.h"
#include "PipelineCommon.h"
#include "PipelineFactory.h"
#include "PipelineStatePresets.h"
#include "ShaderCompiler.h"
#include "Material.h"

#include <array>
#include <cassert>

namespace Ken4lowEngine
{
	/// <summary>
	/// 通常・InstancingのDirectional/Spot/CSM用Depth Pipelineと、
	/// Point Light用の線形距離Depth Pipelineをまとめて保持します。
	/// </summary>
	class ShadowCasterPipelineSet
	{
	public:
		void Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager)
		{
			assert(dxcManager != nullptr);

			const auto inputElements = MakeInputLayout();
			const D3D12_INPUT_LAYOUT_DESC inputLayout{
				inputElements.data(), static_cast<UINT>(inputElements.size())
			};

			const auto& objectDepthVs = Object3DShaderManifest::Get(Object3DShaderId::ShadowMapVS);
			const auto& instancedDepthVs = Object3DShaderManifest::Get(Object3DShaderId::Object3DInstancingShadowVS);
			const auto& objectPointVs = Object3DShaderManifest::Get(Object3DShaderId::Object3DPointShadowVS);
			const auto& instancedPointVs = Object3DShaderManifest::Get(Object3DShaderId::Object3DInstancingPointShadowVS);
			const auto& pointPs = Object3DShaderManifest::Get(Object3DShaderId::Object3DPointShadowPS);

			const ComPtr<IDxcBlob> objectDepthVsBlob = ShaderCompiler::CompileShader(objectDepthVs, dxcManager);
			const ComPtr<IDxcBlob> instancedDepthVsBlob = ShaderCompiler::CompileShader(instancedDepthVs, dxcManager);
			const ComPtr<IDxcBlob> objectPointVsBlob = ShaderCompiler::CompileShader(objectPointVs, dxcManager);
			const ComPtr<IDxcBlob> instancedPointVsBlob = ShaderCompiler::CompileShader(instancedPointVs, dxcManager);
			const ComPtr<IDxcBlob> pointPsBlob = ShaderCompiler::CompileShader(pointPs, dxcManager);

			constexpr std::array<MaterialCullMode, 3> cullModes{
				MaterialCullMode::Back,
				MaterialCullMode::Front,
				MaterialCullMode::None,
			};
			for (size_t i = 0; i < cullModes.size(); ++i)
			{
				CreateObjectDepthPipeline(factory, inputLayout, objectDepthVsBlob, cullModes[i], objectDepthPipelines_[i]);
				CreateInstancedDepthPipeline(factory, inputLayout, instancedDepthVsBlob, cullModes[i], instancedDepthPipelines_[i]);
				CreateObjectPointPipeline(factory, inputLayout, objectPointVsBlob, pointPsBlob, cullModes[i], objectPointPipelines_[i]);
				CreateInstancedPointPipeline(factory, inputLayout, instancedPointVsBlob, pointPsBlob, cullModes[i], instancedPointPipelines_[i]);
			}
		}

		void Finalize()
		{
			for (PipelineBundle& pipeline : instancedPointPipelines_) pipeline.Reset();
			for (PipelineBundle& pipeline : objectPointPipelines_) pipeline.Reset();
			for (PipelineBundle& pipeline : instancedDepthPipelines_) pipeline.Reset();
			for (PipelineBundle& pipeline : objectDepthPipelines_) pipeline.Reset();
		}

		const PipelineBundle& GetObjectDepth(MaterialCullMode cullMode = MaterialCullMode::Back) const { return Select(objectDepthPipelines_, cullMode); }
		const PipelineBundle& GetInstancedDepth(MaterialCullMode cullMode = MaterialCullMode::Back) const { return Select(instancedDepthPipelines_, cullMode); }
		const PipelineBundle& GetObjectPoint(MaterialCullMode cullMode = MaterialCullMode::Back) const { return Select(objectPointPipelines_, cullMode); }
		const PipelineBundle& GetInstancedPoint(MaterialCullMode cullMode = MaterialCullMode::Back) const { return Select(instancedPointPipelines_, cullMode); }

	private:
		static size_t ToIndex(MaterialCullMode cullMode)
		{
			switch (cullMode)
			{
			case MaterialCullMode::Front: return 1;
			case MaterialCullMode::None: return 2;
			case MaterialCullMode::Back:
			default: return 0;
			}
		}

		static const PipelineBundle& Select(const std::array<PipelineBundle, 3>& pipelines, MaterialCullMode cullMode)
		{
			return pipelines[ToIndex(cullMode)];
		}

		static D3D12_RASTERIZER_DESC MakeShadowRasterizer(MaterialCullMode cullMode)
		{
			D3D12_RASTERIZER_DESC rasterizer{};
			switch (cullMode)
			{
			case MaterialCullMode::Front:
				rasterizer = PipelineStatePresets::MakeRasterizerCullFront();
				break;
			case MaterialCullMode::None:
				rasterizer = PipelineStatePresets::MakeRasterizerCullNone();
				break;
			case MaterialCullMode::Back:
			default:
				rasterizer = PipelineStatePresets::MakeRasterizerCullBack();
				break;
			}
			rasterizer.DepthBias = 600;
			rasterizer.SlopeScaledDepthBias = 1.25f;
			rasterizer.DepthBiasClamp = 0.0025f;
			return rasterizer;
		}

		static std::array<D3D12_INPUT_ELEMENT_DESC, 3> MakeInputLayout()
		{
			return {
				D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};
		}

		static GraphicsPipelineDesc MakeDepthDesc(const D3D12_INPUT_LAYOUT_DESC& inputLayout, MaterialCullMode cullMode)
		{
			GraphicsPipelineDesc desc{};
			desc.blendState = PipelineStatePresets::MakeBlendOpaque();
			desc.rasterizerState = MakeShadowRasterizer(cullMode); // ShadowもMain Passと同じMaterial Surface契約を使う。
			desc.depthStencilState = PipelineStatePresets::MakeDepthReadWrite();
			desc.numRenderTargets = 0;
			desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
			desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			desc.sampleCount = 1;
			desc.inputLayout = inputLayout;
			return desc;
		}

		static D3D12_ROOT_SIGNATURE_DESC MakeObjectDepthRoot(std::array<D3D12_ROOT_PARAMETER, 1>& parameters)
		{
			parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			parameters[0].Descriptor.ShaderRegister = 0;
			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(parameters.size());
			desc.pParameters = parameters.data();
			return desc;
		}

		static D3D12_ROOT_SIGNATURE_DESC MakeInstancedDepthRoot(D3D12_DESCRIPTOR_RANGE& instanceRange, std::array<D3D12_ROOT_PARAMETER, 2>& parameters)
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

		static D3D12_ROOT_SIGNATURE_DESC MakeObjectPointRoot(std::array<D3D12_ROOT_PARAMETER, 2>& parameters)
		{
			parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			parameters[0].Descriptor.ShaderRegister = 0;
			parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			parameters[1].Descriptor.ShaderRegister = 1;
			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(parameters.size());
			desc.pParameters = parameters.data();
			return desc;
		}

		static D3D12_ROOT_SIGNATURE_DESC MakeInstancedPointRoot(D3D12_DESCRIPTOR_RANGE& instanceRange, std::array<D3D12_ROOT_PARAMETER, 3>& parameters)
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
			parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			parameters[2].Descriptor.ShaderRegister = 1;
			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = static_cast<UINT>(parameters.size());
			desc.pParameters = parameters.data();
			return desc;
		}

		void CreateObjectDepthPipeline(PipelineFactory& factory, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const ComPtr<IDxcBlob>& vs, MaterialCullMode cullMode, PipelineBundle& destination)
		{
			std::array<D3D12_ROOT_PARAMETER, 1> parameters{};
			const D3D12_ROOT_SIGNATURE_DESC root = MakeObjectDepthRoot(parameters);
			GraphicsPipelineDesc desc = MakeDepthDesc(inputLayout, cullMode);
			desc.debugName = L"ShadowCaster.Object.Depth";
			desc.shaders.vertexShader.blob = vs;
			destination = factory.CreateGraphicsPipeline(desc, root);
		}

		void CreateInstancedDepthPipeline(PipelineFactory& factory, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const ComPtr<IDxcBlob>& vs, MaterialCullMode cullMode, PipelineBundle& destination)
		{
			D3D12_DESCRIPTOR_RANGE instanceRange{};
			std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
			const D3D12_ROOT_SIGNATURE_DESC root = MakeInstancedDepthRoot(instanceRange, parameters);
			GraphicsPipelineDesc desc = MakeDepthDesc(inputLayout, cullMode);
			desc.debugName = L"ShadowCaster.Instanced.Depth";
			desc.shaders.vertexShader.blob = vs;
			destination = factory.CreateGraphicsPipeline(desc, root);
		}

		void CreateObjectPointPipeline(PipelineFactory& factory, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const ComPtr<IDxcBlob>& vs, const ComPtr<IDxcBlob>& ps, MaterialCullMode cullMode, PipelineBundle& destination)
		{
			std::array<D3D12_ROOT_PARAMETER, 2> parameters{};
			const D3D12_ROOT_SIGNATURE_DESC root = MakeObjectPointRoot(parameters);
			GraphicsPipelineDesc desc = MakeDepthDesc(inputLayout, cullMode);
			desc.debugName = L"ShadowCaster.Object.PointLinear";
			desc.shaders.vertexShader.blob = vs;
			desc.shaders.pixelShader.blob = ps;
			destination = factory.CreateGraphicsPipeline(desc, root);
		}

		void CreateInstancedPointPipeline(PipelineFactory& factory, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const ComPtr<IDxcBlob>& vs, const ComPtr<IDxcBlob>& ps, MaterialCullMode cullMode, PipelineBundle& destination)
		{
			D3D12_DESCRIPTOR_RANGE instanceRange{};
			std::array<D3D12_ROOT_PARAMETER, 3> parameters{};
			const D3D12_ROOT_SIGNATURE_DESC root = MakeInstancedPointRoot(instanceRange, parameters);
			GraphicsPipelineDesc desc = MakeDepthDesc(inputLayout, cullMode);
			desc.debugName = L"ShadowCaster.Instanced.PointLinear";
			desc.shaders.vertexShader.blob = vs;
			desc.shaders.pixelShader.blob = ps;
			destination = factory.CreateGraphicsPipeline(desc, root);
		}

	private:
		std::array<PipelineBundle, 3> objectDepthPipelines_{};
		std::array<PipelineBundle, 3> instancedDepthPipelines_{};
		std::array<PipelineBundle, 3> objectPointPipelines_{};
		std::array<PipelineBundle, 3> instancedPointPipelines_{};
	};
}
