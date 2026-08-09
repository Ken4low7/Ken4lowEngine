#pragma once
#include "IPostEffect.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	/// BrightExtract/Blur/Compositeの入口を作る最小Bloomエフェクトです。
	/// -------------------------------------------------------------
	class BloomEffect : public IPostEffect
	{
	private:
		/// BloomEffectのGPU設定です。
		/// 本実装はRT追加を避けるため単一Compute内で軽い近傍BlurとCompositeを行います。
		struct BloomSetting
		{
			float threshold = 1.0f;
			float intensity = 0.0f;
			float blurStrength = 1.0f;
			float padding = 0.0f;
		};

	public:
		/// DirectXリソースとPostEffect用PipelineBuilderから、Compute Pipelineと定数バッファを初期化します。
		void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

		/// GPUリソースを解放し、Map済み定数バッファのポインタを無効化します。
		void Finalize() override;

		/// 入力SRVから明部抽出、簡易Blur、Compositeを行い、出力UAVへ書き込みます。
		void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

		/// PostEffect SettingsへBloomパラメータを表示します。
		void DrawImGui() override;

	private:
		DirectXCommon* dxCommon_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
		BloomSetting* bloomSetting_ = nullptr;
	};

} // namespace Ken4lowEngine
