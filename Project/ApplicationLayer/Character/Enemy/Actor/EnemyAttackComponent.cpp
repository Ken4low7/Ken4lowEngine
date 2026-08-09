#include "EnemyAttackComponent.h"

#include "EnemyAIComponent.h"

#include <Scene/Actor/Character/AttackBehaviors.h>
#include <Scene/Actor/Character/CharacterActor.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	void EnemyAttackComponent::Initialize()
	{
		SetUpdateOrder(-92); // Charge速度を同じフレームのMovement更新へ渡すためAI直後に攻撃を評価する。
		EnsureDefaultAttacks();
		AttackComponent::Initialize(); // タイマー、イベント、攻撃Phaseは共通基盤だけで初期化する。
		lungeDecisionTimer_ = 0.0f;
		lastLungeRoll_ = 0.0f;
		lastSelectedAttack_ = "None";
	}

	void EnemyAttackComponent::Update(float deltaTime)
	{
		lungeDecisionTimer_ = std::max(0.0f, lungeDecisionTimer_ - std::max(0.0f, deltaTime));
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterActor* target = GetTargetActor();
		if (owner && !owner->IsDead() && target && !target->IsDead() && !IsAttacking())
		{
			bool started = false;
			const bool canEvaluateLunge = lungeDecisionTimer_ <= 0.0f &&
				GetCooldownRemaining("LungeScratch") <= 0.0f &&
				IsTargetWithinAttackRange("LungeScratch");
			if (canEvaluateLunge)
			{
				lastLungeRoll_ = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
				lungeDecisionTimer_ = lungeDecisionCooldown_;
				if (lastLungeRoll_ <= lungeChance_)
				{
					started = StartAttack("LungeScratch");
					if (started) lastSelectedAttack_ = "LungeScratch";
				}
			}

			if (!started && IsTargetWithinAttackRange("Melee"))
			{
				started = StartAttack("Melee");
				if (started) lastSelectedAttack_ = "Melee";
			}
		}
		AttackComponent::Update(deltaTime);
	}

	void EnemyAttackComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵攻撃");
		const AttackData* melee = FindAttackData("Melee");
		const AttackData* lunge = FindAttackData("LungeScratch");
		ImGui::Text("Scratch Damage: %.1f / Cooldown: %.2f", melee ? melee->damage : 0.0f, melee ? melee->cooldown : 0.0f);
		ImGui::Text("Lunge Damage: %.1f / Range: %.2f - %.2f", lunge ? lunge->damage : 0.0f, lunge ? lunge->minRange : 0.0f, lunge ? lunge->maxRange : 0.0f);
		ImGui::Text("Lunge確率: %.2f / Roll: %.2f / 再判定: %.2f", lungeChance_, lastLungeRoll_, lungeDecisionTimer_);
		ImGui::Text("選択攻撃: %s / 命中回数: %d", lastSelectedAttack_.c_str(), GetAcceptedHitCount());
		ImGui::Text("実測命中間隔: %.3f", GetLastMeasuredInterval());
#endif
	}

	void EnemyAttackComponent::ToJson(nlohmann::json& outJson) const
	{
		AttackComponent::ToJson(outJson);
		const AttackData* melee = FindAttackData("Melee");
		if (melee)
		{
			outJson["AttackCooldown"] = melee->cooldown;
			outJson["AttackDamage"] = melee->damage;
			outJson["AttackStartDelay"] = melee->windupTime;
			outJson["AttackActiveTime"] = melee->activeTime;
			outJson["AttackRecoveryTime"] = melee->recoveryTime;
			outJson["AttackMaxHeightDifference"] = melee->maxHeightDifference;
		}
		outJson["LungeChance"] = lungeChance_;
		outJson["LungeDecisionCooldown"] = lungeDecisionCooldown_;
	}

	void EnemyAttackComponent::FromJson(const nlohmann::json& inJson)
	{
		AttackComponent::FromJson(inJson);
		EnsureDefaultAttacks();
		if (const AttackData* restored = FindAttackData("Melee"))
		{
			AttackData melee = *restored;
			melee.cooldown = std::max(0.05f, inJson.value("AttackCooldown", melee.cooldown));
			melee.damage = std::max(0.0f, inJson.value("AttackDamage", melee.damage));
			melee.windupTime = std::max(0.0f, inJson.value("AttackStartDelay", melee.windupTime));
			melee.activeTime = std::max(0.0f, inJson.value("AttackActiveTime", melee.activeTime));
			melee.recoveryTime = std::max(0.0f, inJson.value("AttackRecoveryTime", melee.recoveryTime));
			melee.maxHeightDifference = std::max(0.0f, inJson.value("AttackMaxHeightDifference", melee.maxHeightDifference));
			ConfigureAttack("Melee", melee);
		}
		lungeChance_ = std::clamp(inJson.value("LungeChance", lungeChance_), 0.0f, 1.0f);
		lungeDecisionCooldown_ = std::max(0.0f, inJson.value("LungeDecisionCooldown", lungeDecisionCooldown_));
	}

	bool EnemyAttackComponent::ShouldHoldForAttack() const
	{
		if (IsAttacking()) return true;
		if (!GetTargetActor() || GetTargetActor()->IsDead()) return false;
		if (IsTargetWithinAttackRange("Melee")) return true;
		return lungeDecisionTimer_ <= 0.0f &&
			GetCooldownRemaining("LungeScratch") <= 0.0f &&
			IsTargetWithinAttackRange("LungeScratch");
	}

	void EnemyAttackComponent::ApplyDifficultyMultipliers(float cooldownMultiplier, float damageMultiplier)
	{
		const float safeCooldownMultiplier = std::max(0.1f, cooldownMultiplier);
		const float safeDamageMultiplier = std::max(0.1f, damageMultiplier);
		for (const char* attackId : { "Melee", "LungeScratch" })
		{
			const AttackData* current = FindAttackData(attackId);
			if (!current) continue;
			AttackData adjusted = *current;
			adjusted.cooldown = std::max(0.05f, adjusted.cooldown * safeCooldownMultiplier);
			adjusted.damage = std::max(1.0f, adjusted.damage * safeDamageMultiplier);
			ConfigureAttack(attackId, adjusted); // Director倍率は登録データへ一度だけ反映し、攻撃実行側を分岐させない。
		}
	}

	float EnemyAttackComponent::GetAttackCooldown() const
	{
		const AttackData* melee = FindAttackData("Melee");
		return melee ? melee->cooldown : 0.0f;
	}

	float EnemyAttackComponent::GetExpectedHitInterval() const
	{
		const AttackData* melee = FindAttackData("Melee");
		return melee ? melee->windupTime + melee->activeTime + melee->recoveryTime + melee->cooldown : 0.0f;
	}

	float EnemyAttackComponent::GetAttackDamage() const
	{
		const AttackData* melee = FindAttackData("Melee");
		return melee ? melee->damage : 0.0f;
	}

	void EnemyAttackComponent::EnsureDefaultAttacks()
	{
		EnsureDefaultMeleeAttack();
		EnsureDefaultLungeAttack();
	}

	void EnemyAttackComponent::EnsureDefaultMeleeAttack()
	{
		if (FindAttackData("Melee")) return;
		AttackData melee{};
		melee.id = "Melee";
		melee.behaviorType = "Melee";
		melee.animationName = "Attack.Scratch";
		melee.damage = 8.0f;
		melee.cooldown = 0.55f;
		melee.windupTime = 0.12f;
		melee.activeTime = 0.10f;
		melee.recoveryTime = 0.35f;
		melee.maxRange = 2.4f;
		melee.maxHeightDifference = 2.0f;
		RegisterAttack(std::move(melee), std::make_unique<MeleeAttackBehavior>());
	}

	void EnemyAttackComponent::EnsureDefaultLungeAttack()
	{
		if (FindAttackData("LungeScratch")) return;
		AttackData lunge{};
		lunge.id = "LungeScratch";
		lunge.behaviorType = "Charge";
		lunge.animationName = "Attack.LungeScratch";
		lunge.damage = 12.0f;
		lunge.cooldown = 1.4f;
		lunge.windupTime = 0.18f;
		lunge.activeTime = 0.22f;
		lunge.recoveryTime = 0.42f;
		lunge.minRange = 1.6f;
		lunge.maxRange = 3.6f;
		lunge.movementSpeed = 7.0f;
		lunge.maxHeightDifference = 2.0f;
		RegisterAttack(std::move(lunge), std::make_unique<ChargeAttackBehavior>()); // 踏み込み移動は共通Charge Behaviorへ委譲する。
	}
} // namespace Ken4lowEngine
