#pragma once
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <string>
#include <array>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                  コンパイル済みシェーダーバイナリ
	/// -------------------------------------------------------------
	struct ShaderBinary
	{
		/// DXC でコンパイルされたシェーダーバイトコード
		Microsoft::WRL::ComPtr<IDxcBlob> blob;
	};

	/// -------------------------------------------------------------
	///              Graphics / Compute 用シェーダーまとめ
	/// -------------------------------------------------------------
	/// ひとつのパイプラインで使用するシェーダー群を保持する。
	/// Graphics では VS/PS などを、Compute では CS を使う。
	/// 使用しない段は null のままでよい。
	/// -------------------------------------------------------------
	struct ShaderProgram
	{
		ShaderBinary vertexShader;
		ShaderBinary pixelShader;
		ShaderBinary geometryShader;
		ShaderBinary hullShader;
		ShaderBinary domainShader;
		ShaderBinary computeShader;
	};

	/// -------------------------------------------------------------
	///        RootSignature と PipelineState の組み合わせ
	/// -------------------------------------------------------------
	/// 描画時はこの 2 つをセットで使用するため、
	/// ひとまとめにして扱いやすくしている。
	/// -------------------------------------------------------------
	struct PipelineBundle
	{
		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

		/// <summary>
		/// 保持中の D3D12 オブジェクト参照を解放する。
		/// </summary>
		void Reset()
		{
			rootSignature.Reset();
			pipelineState.Reset();
		}
	};

	/// -------------------------------------------------------------
	///            Graphics Pipeline 生成用ディスクリプタ
	/// -------------------------------------------------------------
	/// PipelineFactory に渡して、
	/// Graphics PipelineState を生成するための材料をまとめた構造体。
	/// -------------------------------------------------------------
	struct GraphicsPipelineDesc
	{
		/// デバッグ表示用の名前
		std::wstring debugName;

		/// 使用するシェーダー群
		ShaderProgram shaders{};

		/// IA / Rasterizer / Blend / Depth などの状態
		D3D12_INPUT_LAYOUT_DESC inputLayout{};
		D3D12_BLEND_DESC blendState{};
		D3D12_RASTERIZER_DESC rasterizerState{};
		D3D12_DEPTH_STENCIL_DESC depthStencilState{};

		/// 出力先フォーマット
		std::array<DXGI_FORMAT, 8> rtvFormats{};
		UINT numRenderTargets = 1;
		DXGI_FORMAT dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

		/// 基本描画設定
		D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		UINT sampleMask = D3D12_DEFAULT_SAMPLE_MASK;
		UINT sampleCount = 1;
	};
}