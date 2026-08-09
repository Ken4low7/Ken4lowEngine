#include "SpritePipelineSet.h"
#include "PipelineFactory.h"
#include "PipelineStatePresets.h"
#include "ShaderCompiler.h"
#include "SpriteShaderManifest.h"
#include "DXCCompilerManager.h"
#include "SRVManager.h"
#include <array>
#include <cassert>

namespace Ken4lowEngine
{
	namespace
	{
		/// <summary>
		/// Sprite 頂点用の入力レイアウトを作成する。
		/// 現在の Sprite 頂点は
		/// - POSITION
		/// - TEXCOORD
		/// の 2 要素構成で扱う。
		/// </summary>
		std::array<D3D12_INPUT_ELEMENT_DESC, 2> MakeSpriteInputLayout()
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
			}
			};
		}
	}

	/// <summary>
	/// Sprite 用 RootSignature の定義を組み立てる。
	/// 
	/// ルートパラメータ構成:
	/// - [0] PS b0 : Material
	/// - [1] PS b1 : ReloadProgress などの補助データ
	/// - [2] VS b0 : TransformationMatrix
	/// - [3] PS t0 : テクスチャ SRV テーブル
	/// 
	/// 既存 Sprite 描画コードとの互換を保つため、
	/// SRV テーブルは root parameter index 3 に配置している。
	/// </summary>
	D3D12_ROOT_SIGNATURE_DESC MakeSpriteRootSignatureDesc(
		D3D12_DESCRIPTOR_RANGE* descriptorRange,
		D3D12_ROOT_PARAMETER* rootParameters,
		D3D12_STATIC_SAMPLER_DESC* staticSampler)
	{
		assert(descriptorRange != nullptr);
		assert(rootParameters != nullptr);
		assert(staticSampler != nullptr);

		// PS 側のテクスチャ参照テーブルを定義する
		descriptorRange[0] = {};
		descriptorRange[0].BaseShaderRegister = 0;
		descriptorRange[0].NumDescriptors = SRVManager::GetInstance()->GetkMaxSRVCount();
		descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

		// [0] PixelShader b0 : Material
		rootParameters[0] = {};
		rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[0].Descriptor.ShaderRegister = 0;
		rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// [1] PixelShader b1 : ReloadProgress など
		rootParameters[1] = {};
		rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[1].Descriptor.ShaderRegister = 1;
		rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// [2] VertexShader b0 : TransformationMatrix
		rootParameters[2] = {};
		rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		rootParameters[2].Descriptor.ShaderRegister = 0;
		rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

		// [3] PixelShader t0 : Texture SRV Table
		rootParameters[3] = {};
		rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;
		rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
		rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// Sprite では point sampler を使用する
		*staticSampler = {};
		staticSampler->Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		staticSampler->AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler->AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler->AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		staticSampler->ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		staticSampler->ShaderRegister = 0;
		staticSampler->ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
		staticSampler->MaxLOD = D3D12_FLOAT32_MAX;

		// RootSignature 記述を返す
		D3D12_ROOT_SIGNATURE_DESC desc{};
		desc.NumParameters = 4;
		desc.pParameters = rootParameters;
		desc.NumStaticSamplers = 1;
		desc.pStaticSamplers = staticSampler;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		return desc;
	}

	/// <summary>
	/// Sprite 用 GraphicsPipelineDesc の共通部分を組み立てる。
	/// 背景用 / UI 用で共通な設定をここでまとめて作る。
	/// </summary>
	GraphicsPipelineDesc MakeBaseSpriteDesc(
		DXGI_FORMAT rtvFormat,
		DXGI_FORMAT dsvFormat,
		const D3D12_INPUT_LAYOUT_DESC& inputLayout)
	{
		GraphicsPipelineDesc desc{};

		// Sprite は通常アルファブレンドで描画する
		desc.blendState = PipelineStatePresets::MakeBlendAlpha();

		// 2D スプライトは両面描画でよいのでカリングなし
		desc.rasterizerState = PipelineStatePresets::MakeRasterizerCullNone();

		// 出力先フォーマット
		desc.numRenderTargets = 1;
		desc.rtvFormats[0] = rtvFormat;
		desc.dsvFormat = dsvFormat;

		// 基本の IA / Rasterizer 設定
		desc.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		desc.sampleCount = 1;
		desc.inputLayout = inputLayout;

		return desc;
	}

	void SpritePipelineSet::Initialize(
		PipelineFactory& factory,
		DXCCompilerManager* dxcManager,
		DXGI_FORMAT rtvFormat,
		DXGI_FORMAT dsvFormat)
	{
		assert(dxcManager != nullptr);

		// 1. Sprite 頂点の入力レイアウトを組み立てる
		auto inputElements = MakeSpriteInputLayout();

		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		inputLayout.pInputElementDescs = inputElements.data();
		inputLayout.NumElements = static_cast<UINT>(inputElements.size());

		// 2. Manifest から Sprite 用シェーダー契約を取得する
		const ShaderDescriptor& vsDesc = SpriteShaderManifest::Get(SpriteShaderId::SpriteVS);
		const ShaderDescriptor& psDesc = SpriteShaderManifest::Get(SpriteShaderId::SpritePS);

		// 契約どおりのシェーダー段かを確認する
		assert(vsDesc.stage == ShaderStage::Vertex);
		assert(psDesc.stage == ShaderStage::Pixel);
		assert(vsDesc.rootSignature == RootSignatureType::Sprite);
		assert(psDesc.rootSignature == RootSignatureType::Sprite);

		// 3. シェーダーをコンパイルする
		ComPtr<IDxcBlob> vertexShaderBlob = ShaderCompiler::CompileShader(vsDesc, dxcManager);
		ComPtr<IDxcBlob> pixelShaderBlob = ShaderCompiler::CompileShader(psDesc, dxcManager);

		// 4. RootSignature 記述を作る
		D3D12_DESCRIPTOR_RANGE descriptorRange{};
		D3D12_ROOT_PARAMETER rootParameters[4]{};
		D3D12_STATIC_SAMPLER_DESC staticSampler{};

		D3D12_ROOT_SIGNATURE_DESC rootSigDesc =
			MakeSpriteRootSignatureDesc(&descriptorRange, rootParameters, &staticSampler);

		// 5. 背景スプライト用パイプラインを生成する
		{
			GraphicsPipelineDesc desc = MakeBaseSpriteDesc(rtvFormat, dsvFormat, inputLayout);
			desc.debugName = L"Sprite.Background";
			desc.shaders.vertexShader.blob = vertexShaderBlob;
			desc.shaders.pixelShader.blob = pixelShaderBlob;

			// 背景スプライトは深度を使わない
			desc.depthStencilState = PipelineStatePresets::MakeDepthDisable();

			backgroundPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

			if (backgroundPipeline_.rootSignature)
			{
				backgroundPipeline_.rootSignature->SetName(L"Sprite.Background.RootSignature");
			}
			if (backgroundPipeline_.pipelineState)
			{
				backgroundPipeline_.pipelineState->SetName(L"Sprite.Background.PSO");
			}
		}

		// 6. UI スプライト用パイプラインを生成する
		{
			GraphicsPipelineDesc desc = MakeBaseSpriteDesc(rtvFormat, dsvFormat, inputLayout);
			desc.debugName = L"Sprite.UI";
			desc.shaders.vertexShader.blob = vertexShaderBlob;
			desc.shaders.pixelShader.blob = pixelShaderBlob;

			// 現状の UI も深度無効で扱う
			desc.depthStencilState = PipelineStatePresets::MakeDepthDisable();

			uiPipeline_ = factory.CreateGraphicsPipeline(desc, rootSigDesc);

			if (uiPipeline_.rootSignature)
			{
				uiPipeline_.rootSignature->SetName(L"Sprite.UI.RootSignature");
			}
			if (uiPipeline_.pipelineState)
			{
				uiPipeline_.pipelineState->SetName(L"Sprite.UI.PSO");
			}
		}
	}

	void SpritePipelineSet::Finalize()
	{
		// 保持中の Sprite 用パイプラインを解放する
		backgroundPipeline_.Reset();
		uiPipeline_.Reset();
	}
}