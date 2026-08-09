#pragma once
#include "IPostEffect.h"
#include "Vector4.h"

namespace Ken4lowEngine
{

/// -------------------------------------------------------------
///       プレイヤーHP連動ポストエフェクト
/// -------------------------------------------------------------
class PlayerHealthPostEffect : public IPostEffect
{
public:
	struct Parameters
	{
		float lowHealthVignetteIntensity = 0.0f;
		float damageFlashIntensity = 0.0f;
		Vector4 vignetteColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		float desaturation = 0.0f;
		float darkenIntensity = 0.0f;
		float pulseSpeed = 0.0f;
		float pulseIntensity = 0.0f;
	};

private:
	struct EffectSetting
	{
		Vector4 vignetteColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		float lowHealthVignetteIntensity = 0.0f;
		float damageFlashIntensity = 0.0f;
		float desaturation = 0.0f;
		float darkenIntensity = 0.0f;
		float pulseSpeed = 0.0f;
		float pulseIntensity = 0.0f;
		float elapsedTime = 0.0f;
		float padding = 0.0f;
	};

public:
	void Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* builder) override;
	void Finalize() override;
	void Apply(ID3D12GraphicsCommandList* commandList, uint32_t srvIndex, uint32_t uavIndex, uint32_t dsvIndex) override;
	void DrawImGui() override;

	void SetParameters(const Parameters& parameters);
	const Parameters& GetParameters() const { return parameters_; }
	void SetElapsedTime(float elapsedTime);

private:
	void WriteToConstantBuffer();

private:
	DirectXCommon* dxCommon_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_;
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
	EffectSetting* setting_ = nullptr;
	Parameters parameters_{};
	float elapsedTime_ = 0.0f;
};

} // namespace Ken4lowEngine
