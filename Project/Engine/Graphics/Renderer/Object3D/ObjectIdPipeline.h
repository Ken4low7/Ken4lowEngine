#pragma once

#include "DirectXCommon.h"
#include "Material.h"
#include "PipelineCommon.h"
#include "PipelineFactory.h"
#include "PipelineStatePresets.h"
#include "ShaderCompiler.h"

#include <array>
#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>Editor Picking用のObject-ID描画Pipelineを管理します。</summary>
	class ObjectIdPipeline
	{
	public:
		static ObjectIdPipeline* GetInstance()
		{
			static ObjectIdPipeline instance;
			return &instance;
		}

		void Initialize()
		{
			if (initialized_) return;
			dxCommon_ = DirectXCommon::GetInstance();
			CreateStaticPipelines();
			CreateInstancedPipelines();
			initialized_ = true;
		}

		void Finalize()
		{
			for (PipelineBundle& pipeline : staticPipelines_) pipeline.Reset();
			for (PipelineBundle& pipeline : instancedPipelines_) pipeline.Reset();
			dxCommon_ = nullptr;
			initialized_ = false;
		}

		void BindStatic(ID3D12GraphicsCommandList* commandList, uint32_t objectId, MaterialCullMode cullMode = MaterialCullMode::Back)
		{
			if (!initialized_) Initialize();
			const PipelineBundle& pipeline = staticPipelines_[ToIndex(cullMode)];
			commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
			commandList->SetPipelineState(pipeline.pipelineState.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->SetGraphicsRoot32BitConstant(1, objectId, 0);
		}

		void BindInstanced(ID3D12GraphicsCommandList* commandList, uint32_t baseObjectId, bool addInstanceId, MaterialCullMode cullMode = MaterialCullMode::Back)
		{
			if (!initialized_) Initialize();
			const PipelineBundle& pipeline = instancedPipelines_[ToIndex(cullMode)];
			commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
			commandList->SetPipelineState(pipeline.pipelineState.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			const uint32_t constants[2] = { baseObjectId, addInstanceId ? 1u : 0u };
			commandList->SetGraphicsRoot32BitConstants(2, 2, constants, 0); // Base IDとSV_InstanceID加算フラグをVSへ渡す。
		}

	private:
		ObjectIdPipeline() = default;
		~ObjectIdPipeline() = default;
		ObjectIdPipeline(const ObjectIdPipeline&) = delete;
		ObjectIdPipeline& operator=(const ObjectIdPipeline&) = delete;

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

		static D3D12_RASTERIZER_DESC MakeRasterizer(MaterialCullMode cullMode)
		{
			switch (cullMode)
			{
			case MaterialCullMode::Front: return PipelineStatePresets::MakeRasterizerCullFront();
			case MaterialCullMode::None: return PipelineStatePresets::MakeRasterizerCullNone();
			case MaterialCullMode::Back:
			default: return PipelineStatePresets::MakeRasterizerCullBack();
			}
		}

		static std::array<D3D12_INPUT_ELEMENT_DESC, 3> MakeInputLayout()
		{
			return {
				D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			};
		}

		ShaderProgram CompileProgram(const wchar_t* vertexPath, const wchar_t* pixelPath)
		{
			ShaderProgram program{};
			const ShaderDescriptor vertexDesc{ L"ObjectIdVS", vertexPath, L"main", L"vs_6_0", ShaderStage::Vertex, RootSignatureType::Unknown };
			const ShaderDescriptor pixelDesc{ L"ObjectIdPS", pixelPath, L"main", L"ps_6_0", ShaderStage::Pixel, RootSignatureType::Unknown };
			program.vertexShader.blob = ShaderCompiler::CompileShader(vertexDesc, dxCommon_->GetDXCCompilerManager());
			program.pixelShader.blob = ShaderCompiler::CompileShader(pixelDesc, dxCommon_->GetDXCCompilerManager());
			return program;
		}

		GraphicsPipelineDesc MakeCommonPipelineDesc(const ShaderProgram& program, const D3D12_INPUT_LAYOUT_DESC& inputLayout, const wchar_t* debugName, MaterialCullMode cullMode)
		{
			GraphicsPipelineDesc desc{};
			desc.debugName = debugName;
			desc.shaders = program;
			desc.inputLayout = inputLayout;
			desc.blendState = PipelineStatePresets::MakeBlendOpaque();
			desc.rasterizerState = MakeRasterizer(cullMode); // Picking PassもMain Surfaceと同じ面だけを書き込む。
			desc.depthStencilState = PipelineStatePresets::MakeDepthReadWrite();
			desc.rtvFormats[0] = DXGI_FORMAT_R32_UINT;
			desc.numRenderTargets = 1;
			desc.dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
			desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			return desc;
		}

		void CreateStaticPipelines()
		{
			const auto inputElements = MakeInputLayout();
			const D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements.data(), static_cast<UINT>(inputElements.size()) };
			std::array<D3D12_ROOT_PARAMETER, 2> rootParameters{};
			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[0].Descriptor.ShaderRegister = 0;
			rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[1].Constants.ShaderRegister = 1;
			rootParameters[1].Constants.Num32BitValues = 1;
			D3D12_ROOT_SIGNATURE_DESC rootDesc{};
			rootDesc.NumParameters = static_cast<UINT>(rootParameters.size());
			rootDesc.pParameters = rootParameters.data();
			rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			const ShaderProgram program = CompileProgram(L"Resources/Shaders/EditorPicking/ObjectIdStatic.VS.hlsl", L"Resources/Shaders/EditorPicking/ObjectId.PS.hlsl");
			constexpr std::array<MaterialCullMode, 3> modes{ MaterialCullMode::Back, MaterialCullMode::Front, MaterialCullMode::None };
			for (size_t i = 0; i < modes.size(); ++i)
			{
				GraphicsPipelineDesc desc = MakeCommonPipelineDesc(program, inputLayout, L"ObjectIdStaticPipeline", modes[i]);
				staticPipelines_[i] = dxCommon_->GetPipelineFactory().CreateGraphicsPipeline(desc, rootDesc);
			}
		}

		void CreateInstancedPipelines()
		{
			const auto inputElements = MakeInputLayout();
			const D3D12_INPUT_LAYOUT_DESC inputLayout{ inputElements.data(), static_cast<UINT>(inputElements.size()) };
			D3D12_DESCRIPTOR_RANGE instanceRange{};
			instanceRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			instanceRange.NumDescriptors = 1;
			instanceRange.BaseShaderRegister = 0;
			instanceRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
			std::array<D3D12_ROOT_PARAMETER, 3> rootParameters{};
			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
			rootParameters[0].DescriptorTable.pDescriptorRanges = &instanceRange;
			rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[1].Descriptor.ShaderRegister = 0;
			rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[2].Constants.ShaderRegister = 1;
			rootParameters[2].Constants.Num32BitValues = 2;
			D3D12_ROOT_SIGNATURE_DESC rootDesc{};
			rootDesc.NumParameters = static_cast<UINT>(rootParameters.size());
			rootDesc.pParameters = rootParameters.data();
			rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

			const ShaderProgram program = CompileProgram(L"Resources/Shaders/EditorPicking/ObjectIdInstanced.VS.hlsl", L"Resources/Shaders/EditorPicking/ObjectIdInstanced.PS.hlsl");
			constexpr std::array<MaterialCullMode, 3> modes{ MaterialCullMode::Back, MaterialCullMode::Front, MaterialCullMode::None };
			for (size_t i = 0; i < modes.size(); ++i)
			{
				GraphicsPipelineDesc desc = MakeCommonPipelineDesc(program, inputLayout, L"ObjectIdInstancedPipeline", modes[i]);
				instancedPipelines_[i] = dxCommon_->GetPipelineFactory().CreateGraphicsPipeline(desc, rootDesc);
			}
		}

		DirectXCommon* dxCommon_ = nullptr;
		std::array<PipelineBundle, 3> staticPipelines_{};
		std::array<PipelineBundle, 3> instancedPipelines_{};
		bool initialized_ = false;
	};
} // namespace Ken4lowEngine
