#pragma once
#include "PipelineCommon.h"
#include <d3d12.h>
#include <wrl.h>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///     RootSignature / GraphicsPipelineState を生成するクラス
	/// -------------------------------------------------------------
	/// このクラスは「どう描画するか」を決めるのではなく、
	/// 渡された設定をもとに D3D12 のパイプラインオブジェクトを作る。
	///
	/// つまり、
	/// - どのシェーダーを使うか
	/// - どの Blend / Depth 設定を使うか
	/// は呼び出し側が決める。
	/// -------------------------------------------------------------
	class PipelineFactory
	{
	public: /// ---------- メンバ関数 ---------- ///

		PipelineFactory() = default;
		explicit PipelineFactory(ID3D12Device* device);

		/// <summary>
		/// 使用する D3D12 デバイスを設定する。
		/// </summary>
		void Initialize(ID3D12Device* device);

		/// <summary>
		/// 保持中のデバイス参照を解放する。
		/// </summary>
		void Finalize();

		/// <summary>
		/// Graphics 用 RootSignature / PSO を生成して返す。
		/// </summary>
		/// <param name="desc">
		/// パイプライン生成に必要な設定群。
		/// </param>
		/// <param name="rootSignatureDesc">
		/// RootSignature の定義。
		/// </param>
		PipelineBundle CreateGraphicsPipeline(
			const GraphicsPipelineDesc& desc,
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

	private: /// ---------- 内部関数 ---------- ///

		/// <summary>
		/// RootSignature の定義をシリアライズする。
		/// </summary>
		Microsoft::WRL::ComPtr<ID3DBlob> SerializeRootSignature(
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

	private: /// ---------- メンバ変数 ---------- ///

		/// パイプライン生成先となる D3D12 デバイス
		Microsoft::WRL::ComPtr<ID3D12Device> device_;
	};
}