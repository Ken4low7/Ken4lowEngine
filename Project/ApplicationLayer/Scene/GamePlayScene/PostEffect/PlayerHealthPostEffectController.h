#pragma once
#include "Effects/PlayerHealthPostEffect/PlayerHealthPostEffect.h"
#include "IPlayerRuntime.h"

namespace Ken4lowEngine
{
	class PostEffectManager;
}

/// -------------------------------------------------------------
/// プレイヤーHPから画面危機演出用パラメータを計算するクラス
/// -------------------------------------------------------------
class PlayerHealthPostEffectController
{
public:
	void Initialize(Ken4lowEngine::PostEffectManager* postEffectManager);
	void Finalize();
	void Update(float deltaTime, const IPlayerRuntime* player);
	void DrawImGui();
	void DrawImGuiContent();
	void NotifyDamageTaken();

	bool IsEnabled() const { return enabled_; }
	float GetHpRate() const { return hpRate_; }

private:
	Ken4lowEngine::PlayerHealthPostEffect* GetEffect() const;
	Ken4lowEngine::PlayerHealthPostEffect::Parameters BuildParameters() const;
	float CalculateLowHealthVignetteIntensity() const;
	float CalculateLowRate01(float threshold) const;
	void ApplyToPostEffect();

private:
	Ken4lowEngine::PostEffectManager* postEffectManager_ = nullptr;
	bool enabled_ = true;
	float hpRate_ = 1.0f;
	float elapsedTime_ = 0.0f;

	float redVignetteStrength_ = 0.35f;
	float damageFlashStrength_ = 0.28f;
	float damageFlashDecaySpeed_ = 3.8f;
	float desaturationStrength_ = 0.18f;
	float darkenStrength_ = 0.16f;
	float pulseSpeed_ = 5.0f;
	float pulseIntensity_ = 0.18f;
	float lowHpThreshold_ = 0.70f;
	float dangerHpThreshold_ = 0.20f;
	float strongHpThreshold_ = 0.40f;
	float damageFlashIntensity_ = 0.0f;
};
