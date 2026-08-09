#include "SkyBoxPipelineSet.h"
#include "PipelineFactory.h"
#include "PipelineStatePresets.h"
#include "ShaderCompiler.h"
#include "SkyBoxShaderManifest.h"
#include "DXCCompilerManager.h"
#include "SRVManager.h"
#include <array>
#include <cassert>

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// SkyBox 頂点用入力レイアウトを作成する。
		/// SkyBox::VertexData に合わせて
		/// - POSITION : float4
		/// - TEXCOORD : float3
		/// で定義する。
		/// </summary>
		std::array<D3D12_INPUT_ELEMENT_DESC, 2> MakeSkyBoxInputLayout()
		{
			return {
				D3D12_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
				D3D12_INPUT_ELEMENT_DESC{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,	 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
			};
		}

		/// <summary>
		/// SkyBox 用 RootSignature の定義を作成する。
		/// ルートパラメータ構成:
		/// [0] PS b0 : Material
		/// [1] VS b0 : TransformationMatrix
		/// [2] PS t0 : 環境テクスチャ SRV テーブル
		/// [3] PS t1024 : 2D 雲テクスチャ SRV テーブル
		/// </summary>
		D3D12_ROOT_SIGNATURE_DESC MakeSkyBoxRootSignatureDesc(D3D12_DESCRIPTOR_RANGE* descriptorRanges, D3D12_ROOT_PARAMETER* rootParameters, D3D12_STATIC_SAMPLER_DESC* staticSampler)
		{
			assert(descriptorRanges != nullptr);
			assert(rootParameters != nullptr);
			assert(staticSampler != nullptr);

			descriptorRanges[0] = {};
			descriptorRanges[0].BaseShaderRegister = 0;
			descriptorRanges[0].NumDescriptors = SRVManager::GetInstance()->GetkMaxSRVCount();
			descriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			descriptorRanges[1] = descriptorRanges[0];
			descriptorRanges[1].BaseShaderRegister = SRVManager::GetInstance()->GetkMaxSRVCount();

			rootParameters[0] = {};
			rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[0].Descriptor.ShaderRegister = 0;

			rootParameters[1] = {};
			rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
			rootParameters[1].Descriptor.ShaderRegister = 0;

			rootParameters[2] = {};
			rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			rootParameters[2].DescriptorTable.pDescriptorRanges = &descriptorRanges[0];
			rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

			rootParameters[3] = rootParameters[2];
			rootParameters[3].DescriptorTable.pDescriptorRanges = &descriptorRanges[1];

			*staticSampler = {};
			staticSampler->Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
			staticSampler->AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSampler->AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSampler->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			staticSampler->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			staticSampler->ShaderRegister = 0;
			staticSampler->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
			staticSampler->MaxLOD = D3D12_FLOAT32_MAX;

			D3D12_ROOT_SIGNATURE_DESC desc{};
			desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
			desc.NumParameters = 4;
			desc.pParameters = rootParameters;
			desc.NumStaticSamplers = 1;
			desc.pStaticSamplers = staticSampler;
			return desc;
		}

		/// <summary>
		/// SkyBox 用 GraphicsPipelineDesc の共通部を組み立てる。
		/// </summary>
		GraphicsPipelineDesc MakeBaseSkyBoxDesc(DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat, const D3D12_INPUT_LAYOUT_DESC& inputLayout)
		{
			GraphicsPipelineDesc desc{};
			desc.blendState = PipelineStatePresets::MakeBlendOpaque();
			desc.rasterizerState = PipelineStatePresets::MakeRasterizerCullBack();
			desc.rasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
			desc.depthStencilState = PipelineStatePresets::MakeDepthReadOnly();

			desc.numRenderTargets = 1;
			desc.rtvFormats[0] = rtvFormat;
			desc.dsvFormat = dsvFormat;
			desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			desc.sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
			desc.sampleCount = 1;
			desc.inputLayout = inputLayout;
			return desc;
		}
	}

	void SkyBoxPipelineSet::Initialize(PipelineFactory& factory, DXCCompilerManager* dxcManager, DXGI_FORMAT rtvFormat, DXGI_FORMAT dsvFormat)
	{
		assert(dxcManager != nullptr);

		auto inputElements = MakeSkyBoxInputLayout();

		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		inputLayout.pInputElementDescs = inputElements.data();
		inputLayout.NumElements = static_cast<UINT>(inputElements.size());

		const ShaderDescriptor& vsDesc = SkyBoxShaderManifest::Get(SkyBoxShaderId::SkyBoxVS);
		const ShaderDescriptor& psDesc = SkyBoxShaderManifest::Get(SkyBoxShaderId::SkyBoxPS);

		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);

		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(vsDesc, dxcManager);
		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(psDesc, dxcManager);

		D3D12_DESCRIPTOR_RANGE descriptorRanges[2]{};
		D3D12_ROOT_PARAMETER rootParameters[4]{};
		D3D12_STATIC_SAMPLER_DESC staticSampler{};

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc = MakeSkyBoxRootSignatureDesc(descriptorRanges, rootParameters, &staticSampler);

		GraphicsPipelineDesc desc = MakeBaseSkyBoxDesc(rtvFormat, dsvFormat, inputLayout);
		desc.debugName = L"SkyBox.Default";
		desc.shaders.vertexShader.blob = vertexShaderBlob;
		desc.shaders.pixelShader.blob = pixelShaderBlob;

		defaultPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

		desc.blendState = PipelineStatePresets::MakeBlendAlpha();
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.debugName = L"SkyBox.Cloud";
		cloudPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

		if (defaultPipeline_.rootSignature)
		{
			defaultPipeline_.rootSignature->SetName(L"SkyBox.Default.RootSignature");
		}
		if (defaultPipeline_.pipelineState)
		{
			defaultPipeline_.pipelineState->SetName(L"SkyBox.Default.PSO");
		}
	}

	void SkyBoxPipelineSet::Finalize()
	{
		defaultPipeline_.Reset();
		cloudPipeline_.Reset();
	}
}
