#define NOMINMAX
#include "PlayerHealthPostEffectController.h"

#include "PostEffectManager.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace
{
	constexpr const char* kEffectName = "PlayerHealthPostEffect";
}

void PlayerHealthPostEffectController::Initialize(Ken4lowEngine::PostEffectManager* postEffectManager)
{
	postEffectManager_ = postEffectManager;
	hpRate_ = 1.0f;
	damageFlashIntensity_ = 0.0f;
	elapsedTime_ = 0.0f;
	ApplyToPostEffect();
}

void PlayerHealthPostEffectController::Finalize()
{
	if (postEffectManager_)
	{
		postEffectManager_->DisableEffect(kEffectName);
	}
	postEffectManager_ = nullptr;
}

void PlayerHealthPostEffectController::Update(float deltaTime, const IPlayerRuntime* player)
{
	elapsedTime_ += deltaTime;

	if (player && player->GetMaxHP() > 0.0f)
	{
		hpRate_ = std::clamp(player->GetHP() / player->GetMaxHP(), 0.0f, 1.0f);
	}
	else
	{
		hpRate_ = 1.0f;
	}

	// 被弾直後の赤フラッシュは毎フレーム少しずつ弱める。
	damageFlashIntensity_ = std::max(0.0f, damageFlashIntensity_ - damageFlashDecaySpeed_ * deltaTime);
	ApplyToPostEffect();
}

void PlayerHealthPostEffectController::DrawImGui()
{
#ifdef USE_IMGUI
	// 単体表示時もDocking可能な通常ウィンドウとして開く。
	if (ImGui::Begin("Player Health Post Effect"))
	{
		DrawImGuiContent();
	}
	ImGui::End();
#endif // USE_IMGUI
}

void PlayerHealthPostEffectController::DrawImGuiContent()
{
#ifdef USE_IMGUI
	// Player Debug内にHP/Damage系の調整を埋め込めるよう中身だけ分離する。
	ImGui::Checkbox("HPポストエフェクト有効", &enabled_);
	ImGui::SliderFloat("HP率", &hpRate_, 0.0f, 1.0f);
	ImGui::SliderFloat("赤ビネット強度", &redVignetteStrength_, 0.0f, 1.0f);
	ImGui::SliderFloat("被弾フラッシュ強度", &damageFlashStrength_, 0.0f, 1.0f);
	ImGui::SliderFloat("被弾フラッシュ減衰速度", &damageFlashDecaySpeed_, 0.1f, 10.0f);
	ImGui::SliderFloat("彩度低下", &desaturationStrength_, 0.0f, 1.0f);
	ImGui::SliderFloat("暗さ", &darkenStrength_, 0.0f, 1.0f);
	ImGui::SliderFloat("脈動速度", &pulseSpeed_, 0.0f, 12.0f);
	ImGui::SliderFloat("脈動強度", &pulseIntensity_, 0.0f, 0.5f);
	ImGui::SliderFloat("低HPしきい値", &lowHpThreshold_, 0.01f, 1.0f);
	ImGui::SliderFloat("危険HPしきい値", &dangerHpThreshold_, 0.01f, 1.0f);
	lowHpThreshold_ = std::max(lowHpThreshold_, dangerHpThreshold_ + 0.01f);
	strongHpThreshold_ = std::clamp(strongHpThreshold_, dangerHpThreshold_ + 0.01f, lowHpThreshold_);

	ApplyToPostEffect();
#endif // USE_IMGUI
}

void PlayerHealthPostEffectController::NotifyDamageTaken()
{
	damageFlashIntensity_ = std::max(damageFlashIntensity_, damageFlashStrength_);
	ApplyToPostEffect();
}

Ken4lowEngine::PlayerHealthPostEffect* PlayerHealthPostEffectController::GetEffect() const
{
	if (!postEffectManager_)
	{
		return nullptr;
	}

	return dynamic_cast<Ken4lowEngine::PlayerHealthPostEffect*>(postEffectManager_->GetEffect(kEffectName));
}

Ken4lowEngine::PlayerHealthPostEffect::Parameters PlayerHealthPostEffectController::BuildParameters() const
{
	Ken4lowEngine::PlayerHealthPostEffect::Parameters params{};
	params.lowHealthVignetteIntensity = CalculateLowHealthVignetteIntensity();
	params.damageFlashIntensity = std::clamp(damageFlashIntensity_, 0.0f, 1.0f);
	params.vignetteColor = { 1.0f, 0.02f, 0.02f, 1.0f };

	const float dangerRate01 = CalculateLowRate01(dangerHpThreshold_);
	params.desaturation = std::clamp(desaturationStrength_ * dangerRate01, 0.0f, 0.35f);
	params.darkenIntensity = std::clamp(darkenStrength_ * dangerRate01, 0.0f, 0.30f);
	params.pulseSpeed = (hpRate_ <= dangerHpThreshold_) ? pulseSpeed_ : 0.0f;
	params.pulseIntensity = (hpRate_ <= dangerHpThreshold_) ? pulseIntensity_ : 0.0f;

	return params;
}

float PlayerHealthPostEffectController::CalculateLowHealthVignetteIntensity() const
{
	if (hpRate_ > lowHpThreshold_)
	{
		return 0.0f;
	}

	const float lowRate01 = CalculateLowRate01(lowHpThreshold_);
	const float strongRate01 = CalculateLowRate01(strongHpThreshold_);
	const float dangerRate01 = CalculateLowRate01(dangerHpThreshold_);
	const float baseIntensity = 0.35f * lowRate01 + 0.45f * strongRate01 + 0.20f * dangerRate01;
	return std::clamp(baseIntensity * redVignetteStrength_, 0.0f, 0.75f);
}

float PlayerHealthPostEffectController::CalculateLowRate01(float threshold) const
{
	if (threshold <= 0.0f || hpRate_ >= threshold)
	{
		return 0.0f;
	}

	return std::clamp((threshold - hpRate_) / threshold, 0.0f, 1.0f);
}

void PlayerHealthPostEffectController::ApplyToPostEffect()
{
	if (!postEffectManager_)
	{
		return;
	}

	if (enabled_)
	{
		postEffectManager_->EnableEffect(kEffectName);
	}
	else
	{
		postEffectManager_->DisableEffect(kEffectName);
	}

	if (auto* effect = GetEffect())
	{
		effect->SetParameters(BuildParameters());
		effect->SetElapsedTime(elapsedTime_);
	}
}
