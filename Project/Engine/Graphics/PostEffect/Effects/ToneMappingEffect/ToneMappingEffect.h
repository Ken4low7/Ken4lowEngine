#pragma once
#include "IPostEffect.h"

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	/// HDR/LDR境界を扱う最小ToneMappingエフェクトです。
	/// -------------------------------------------------------------
	class ToneMappingEffect : public IPostEffect
	{
	private:
		/// ToneMappingEffectのGPU設定です。
		/// HDR RenderTarget全面移行前でも既存LDR入力へ通せるよう、初期値は見た目を大きく変えない値にします。
		struct ToneMappingSetting
		{
			float exposure = 1.0f;
			float gamma = 2.2f;
			uint32_t toneMappingType = 0; // 0: None, 1: Reinhard
			float padding = 0.0f;
		};

	public:
		/// DirectXリソースとPostEffect用PipelineBuilderから、Compute Pipelineと定数バッファを初期化します。
		void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;

		/// GPUリソースを解放し、Map済み定数バッファのポインタを無効化します。
		void Finalize() override;

		/// 入力SRVをToneMappingして出力UAVへ書き込みます。
		void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;

		/// PostEffect Settingsへexposure/gamma/typeを表示します。
		void DrawImGui() override;

	private:
		DirectXCommon* dxCommon_ = nullptr;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
		Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
		ToneMappingSetting* toneMappingSetting_ = nullptr;
	};

} // namespace Ken4lowEngine
