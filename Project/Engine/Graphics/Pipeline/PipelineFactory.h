#pragma once
#include "PipelineCommon.h"
#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

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
	public: /// ---------- 型定義 ---------- ///

		struct PipelineCacheStats
		{
			uint64_t requestCount = 0;
			uint64_t hitCount = 0;
			uint64_t missCount = 0;
			uint64_t createCount = 0;
			uint64_t clearCount = 0;
			uint32_t entryCount = 0;
		};

	public: /// ---------- メンバ関数 ---------- ///

		PipelineFactory() = default;
		explicit PipelineFactory(ID3D12Device* device);

		/// <summary>
		/// 使用する D3D12 デバイスを設定する。
		/// </summary>
		void Initialize(ID3D12Device* device);

		/// <summary>
		/// PSO Cacheを破棄してから保持中のデバイス参照を解放する。
		/// </summary>
		void Finalize();

		/// <summary>
		/// Graphics 用 RootSignature / PSO を生成して返す。同一構造KeyはCacheから共有する。
		/// </summary>
		PipelineBundle CreateGraphicsPipeline(
			const GraphicsPipelineDesc& desc,
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

		/// <summary>Graphics Pipeline Cacheを明示的に全破棄します。</summary>
		void ClearCache();

		PipelineCacheStats GetCacheStats() const;

	private: /// ---------- 内部関数 ---------- ///

		/// <summary>
		/// RootSignature の定義をシリアライズする。
		/// </summary>
		Microsoft::WRL::ComPtr<ID3DBlob> SerializeRootSignature(
			const D3D12_ROOT_SIGNATURE_DESC& rootSignatureDesc) const;

		/// <summary>
		/// Serialized RootSignature / Shader bytecode / fixed-function stateからPointer非依存Keyを作る。
		/// </summary>
		std::string BuildGraphicsPipelineCacheKey(
			const GraphicsPipelineDesc& desc,
			ID3DBlob* serializedRootSignature) const;

	private: /// ---------- メンバ変数 ---------- ///

		/// パイプライン生成先となる D3D12 デバイス
		Microsoft::WRL::ComPtr<ID3D12Device> device_;

		// Phase 9.6ではRootSignature pointerではなくSerialized内容とPSO構造で再利用可否を決める。
		mutable std::mutex cacheMutex_;
		mutable std::unordered_map<std::string, PipelineBundle> graphicsPipelineCache_;
		mutable PipelineCacheStats cacheStats_{};
	};
}