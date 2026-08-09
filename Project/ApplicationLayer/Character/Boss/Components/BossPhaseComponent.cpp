#include "BossPhaseComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void BossPhaseComponent::Initialize()
	{
		SanitizeSettings();
		currentPhase_ = ToBossPhase(ToInt(currentPhase_));
		healthCapacityApplied_ = false;
		phaseInvulnerabilityRemaining_ = 0.0f;
		phaseInvulnerabilityActive_ = false;
		wasInvulnerableBeforePhase_ = false;
	}

	void BossPhaseComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr;
		if (!health) return;

		ApplyConfiguredHealthCapacity(*health);
		UpdatePhaseInvulnerability(*health, deltaTime);
		if (health->IsDead() || phaseInvulnerabilityActive_) return;

		const BossPhase evaluatedPhase = EvaluatePhase(health->GetHealthRatio());
		if (ToInt(evaluatedPhase) <= ToInt(currentPhase_)) return; // 回復しても戦闘フェーズは巻き戻さない。

		const int nextPhaseNumber = std::min(ToInt(evaluatedPhase), ToInt(currentPhase_) + 1);
		currentPhase_ = ToBossPhase(nextPhaseNumber); // 大ダメージでもPhase 2とPhase 3を順番に見せる。
		++phaseRevision_;
		BeginPhaseInvulnerability(*health);
	}

	void BossPhaseComponent::Finalize()
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		if (CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr)
		{
			if (phaseInvulnerabilityActive_) EndPhaseInvulnerability(*health);
		}
		healthCapacityApplied_ = false;
		phaseInvulnerabilityRemaining_ = 0.0f;
		phaseInvulnerabilityActive_ = false;
		wasInvulnerableBeforePhase_ = false;
	}

	void BossPhaseComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ボスフェーズ");
		ImGui::Text("最大HP設定: %.0f", configuredMaxHealth_);
		ImGui::Text("現在: Phase %d / Revision %u", ToInt(currentPhase_), phaseRevision_);
		ImGui::Text("Phase 2: HP %.0f%% / Phase 3: HP %.0f%%", phase2HealthRatio_ * 100.0f, phase3HealthRatio_ * 100.0f);
		ImGui::Text("Phase 3: %s", phase3Enabled_ ? "有効" : "無効");
		ImGui::Text("移行無敵: %s %.2f / %.2f", phaseInvulnerabilityActive_ ? "有効" : "無効", phaseInvulnerabilityRemaining_, phaseInvulnerabilityDuration_);
#endif
	}

	void BossPhaseComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["ConfiguredMaxHealth"] = configuredMaxHealth_;
		outJson["Phase2HealthRatio"] = phase2HealthRatio_;
		outJson["Phase3HealthRatio"] = phase3HealthRatio_;
		outJson["PhaseInvulnerabilityDuration"] = phaseInvulnerabilityDuration_;
		outJson["CurrentPhase"] = ToInt(currentPhase_);
	}

	void BossPhaseComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		configuredMaxHealth_ = inJson.value("ConfiguredMaxHealth", configuredMaxHealth_);
		phase2HealthRatio_ = inJson.value("Phase2HealthRatio", phase2HealthRatio_);
		phase3HealthRatio_ = inJson.value("Phase3HealthRatio", phase3HealthRatio_);
		phaseInvulnerabilityDuration_ = inJson.value("PhaseInvulnerabilityDuration", phaseInvulnerabilityDuration_);
		const int currentPhaseNumber = inJson.value("CurrentPhase", ToInt(currentPhase_));
		currentPhase_ = ToBossPhase(currentPhaseNumber); // JSONでは既存Prefab互換のため数値を維持し、読込直後に列挙型へ変換する。
		SanitizeSettings();
		healthCapacityApplied_ = false;
	}

	void BossPhaseComponent::SetPhase3Enabled(bool enabled)
	{
		if (phase3Enabled_ == enabled) return;
		phase3Enabled_ = enabled;
		if (!phase3Enabled_ && currentPhase_ == BossPhase::Phase3)
		{
			currentPhase_ = BossPhase::Phase2;
			++phaseRevision_; // 実行中に無効化された場合もPresentation側へPhase 2への補正を通知する。
		}
	}

	void BossPhaseComponent::ResetPhase()
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		if (CharacterHealthComponent* health = owner ? owner->GetHealthComponent() : nullptr)
		{
			if (phaseInvulnerabilityActive_) EndPhaseInvulnerability(*health);
		}
		currentPhase_ = BossPhase::Phase1;
		healthCapacityApplied_ = false;
		phaseInvulnerabilityRemaining_ = 0.0f;
		++phaseRevision_; // PresentationへReset後の状態再同期を通知する。
	}

	BossPhase BossPhaseComponent::EvaluatePhase(float healthRatio) const
	{
		if (phase3Enabled_ && healthRatio <= phase3HealthRatio_) return BossPhase::Phase3; // Phase 3は許可されたステージだけで判定する。
		if (healthRatio <= phase2HealthRatio_) return BossPhase::Phase2;
		return BossPhase::Phase1;
	}

	void BossPhaseComponent::SanitizeSettings()
	{
		configuredMaxHealth_ = std::isfinite(configuredMaxHealth_) ? std::clamp(configuredMaxHealth_, 1.0f, 100000.0f) : 2400.0f;
		phase2HealthRatio_ = std::isfinite(phase2HealthRatio_) ? std::clamp(phase2HealthRatio_, 0.01f, 1.0f) : 0.70f;
		phase3HealthRatio_ = std::isfinite(phase3HealthRatio_) ? std::clamp(phase3HealthRatio_, 0.0f, phase2HealthRatio_) : 0.35f;
		phaseInvulnerabilityDuration_ = std::isfinite(phaseInvulnerabilityDuration_)
			? std::clamp(phaseInvulnerabilityDuration_, 0.10f, 5.0f)
			: 1.20f;
	}

	void BossPhaseComponent::ApplyConfiguredHealthCapacity(CharacterHealthComponent& health)
	{
		if (healthCapacityApplied_) return;
		const float previousMaxHealth = std::max(1.0f, health.GetMaxHealth());
		const float previousRatio = std::clamp(health.GetCurrentHealth() / previousMaxHealth, 0.0f, 1.0f);
		health.SetMaxHealth(configuredMaxHealth_);
		health.SetCurrentHealth(configuredMaxHealth_ * previousRatio); // Prefab再読込時は受けたDamage割合を維持し、初回生成は2400で開始する。
		healthCapacityApplied_ = true;
	}

	void BossPhaseComponent::BeginPhaseInvulnerability(CharacterHealthComponent& health)
	{
		wasInvulnerableBeforePhase_ = health.IsInvulnerable();
		phaseInvulnerabilityRemaining_ = phaseInvulnerabilityDuration_;
		phaseInvulnerabilityActive_ = true;
		health.SetInvulnerable(true); // フェーズ演出と次の行動準備中に連射で削り切られないようDamageを遮断する。
	}

	void BossPhaseComponent::UpdatePhaseInvulnerability(CharacterHealthComponent& health, float deltaTime)
	{
		if (!phaseInvulnerabilityActive_) return;
		health.SetInvulnerable(true);
		if (std::isfinite(deltaTime) && deltaTime > 0.0f)
		{
			phaseInvulnerabilityRemaining_ = std::max(0.0f, phaseInvulnerabilityRemaining_ - deltaTime);
		}
		if (phaseInvulnerabilityRemaining_ <= 0.0f || health.IsDead()) EndPhaseInvulnerability(health);
	}

	void BossPhaseComponent::EndPhaseInvulnerability(CharacterHealthComponent& health)
	{
		health.SetInvulnerable(wasInvulnerableBeforePhase_);
		phaseInvulnerabilityRemaining_ = 0.0f;
		phaseInvulnerabilityActive_ = false;
		wasInvulnerableBeforePhase_ = false;
	}
} // namespace Ken4lowEngine
