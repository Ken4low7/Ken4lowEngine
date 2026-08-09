#pragma once

#include "ApplicationLayer/Character/CharacterWorld/CharacterWorld.h"
#include "ApplicationLayer/Character/Enemy/Actor/EnemyActor.h"
#include "ApplicationLayer/Character/Player/IPlayerRuntime.h"
#include "ApplicationLayer/Scene/GamePlayScene/Core/GamePlayStageContext.h"

#include <Collider.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/CharacterTargetComponent.h>
#include <SceneComponent.h>
#include <WorldGaugeComponent.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

namespace K4E = ::Ken4lowEngine;

/// Stage 3の中央設備をDamage受付可能なCharacterとして扱い、頭上へ常時HPゲージを表示する。
class Stage3DefenseTargetActor final : public K4E::CharacterActor
{
public:
	std::string GetClassTypeName() const override { return "Stage3DefenseTargetActor"; }

	void Initialize() override
	{
		K4E::SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<K4E::SceneComponent>();
			root->SetName("Stage 3 Defense Root");
			root->SetUpdateOrder(-100);
		}

		if (!GetComponent<K4E::WorldGaugeComponent>())
		{
			auto& gauge = AddComponent<K4E::WorldGaugeComponent>();
			gauge.SetName("Defense Core HP Gauge");
			gauge.SetDrawOrder(150);
			gauge.SetLocalPosition({ 0.0f, 8.6f, 0.0f });
			gauge.SetScreenOffset({ 0.0f, -18.0f });
			gauge.SetSize({ 360.0f, 24.0f });
			gauge.SetBackgroundColor({ 0.025f, 0.035f, 0.055f, 0.92f });
			gauge.SetFillColor({ 0.20f, 0.78f, 1.0f, 1.0f });
			gauge.SetBorderColor({ 0.82f, 0.94f, 1.0f, 0.96f });
			gauge.SetBorderThickness(2.0f);
			gauge.SetHideWhenBehindCamera(true);
			gauge.SetVisible(true);
			gauge.AttachTo(root);
		}

		K4E::CharacterActor::Initialize();
		gauge_ = GetComponent<K4E::WorldGaugeComponent>();
		if (K4E::CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(maxHealth_);
		if (K4E::CharacterMovementComponent* movement = GetMovementComponent()) movement->SetMovementEnabled(false);
		if (K4E::CharacterTargetComponent* target = GetTargetComponent())
		{
			target->SetLocalPosition({ 0.0f, 1.2f, 6.6f }); // 既存コア外周の手前を攻撃点にして、敵が中央Colliderへ押し付けられるのを防ぐ。
		}
		if (K4E::CharacterColliderComponent* collider = GetColliderComponent())
		{
			collider->SetHalfSize({ 5.2f, 4.0f, 5.2f });
			collider->SetActive(false);
			if (K4E::Collider* primitive = collider->GetCollider()) primitive->SetEnabled(false);
		}
		SetName("Stage3DefenseCore");
		SetLayer("StageObjective");
		AddTag("DefenseTarget");
		SyncGauge();
	}

	void Update(float deltaTime) override
	{
		K4E::CharacterActor::Update(deltaTime);
		SyncGauge();
	}

	void Configure(const K4E::Vector3& position, float maxHealth)
	{
		maxHealth_ = std::max(1.0f, maxHealth);
		if (K4E::CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(maxHealth_);
		if (K4E::SceneComponent* root = GetRootComponent())
		{
			root->SetLocalPosition(position);
			root->RefreshWorldTransform();
		}
		SyncGauge();
	}

	float GetHP() const
	{
		const K4E::CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetCurrentHealth() : 0.0f;
	}

	float GetMaxHP() const
	{
		const K4E::CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetMaxHealth() : maxHealth_;
	}

	float GetHealthRatio() const
	{
		const float maxHp = GetMaxHP();
		return maxHp > 0.0f ? std::clamp(GetHP() / maxHp, 0.0f, 1.0f) : 0.0f;
	}

protected:
	void OnDeath(const K4E::CharacterDeathEvent& deathEvent) override
	{
		(void)deathEvent;
		if (K4E::CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		SyncGauge();
	}

private:
	void SyncGauge()
	{
		if (!gauge_) return;
		gauge_->SetMaxValue(GetMaxHP());
		gauge_->SetValue(GetHP());
		gauge_->SetFillColor(GetHealthRatio() <= 0.30f
			? K4E::Vector4{ 1.0f, 0.20f, 0.12f, 1.0f }
			: K4E::Vector4{ 0.20f, 0.78f, 1.0f, 1.0f });
		gauge_->SetVisible(true);
	}

	K4E::WorldGaugeComponent* gauge_ = nullptr;
	float maxHealth_ = 2200.0f;
};

/// Stage 3の防衛対象生成、EnemyのTarget切替、中央へ攻め込む増援を管理する。
class Stage3DefenseRuntime final
{
public:
	struct PromptSnapshot
	{
		bool visible = false;
		std::string text;
		float normalizedProgress = 0.0f;
	};

	static constexpr const char* GetDestroyedEventId() { return "__Stage3DefenseTargetDestroyed"; }

	void Initialize(const GamePlayStageContext& stageContext)
	{
		Finalize();
		const auto rule = stageContext.GetCurrentStageRule();
		active_ = rule.objectiveType == GamePlayStageContext::StageObjectiveType::DefendTarget;
		if (!active_) return;
		if (!stageContext.GetDefenseTargetPoints().empty()) targetPosition_ = stageContext.GetDefenseTargetPoints().front().position;
		else targetPosition_ = { 0.0f, 0.0f, 0.0f };
		targetPosition_.y = std::max(1.5f, targetPosition_.y);
	}

	void Finalize()
	{
		if (target_) target_->Destroy();
		target_ = nullptr;
		prompt_ = {};
		elapsedSec_ = 0.0f;
		spawnTimer_ = 1.5f;
		retargetTimer_ = 0.0f;
		spawnSequence_ = 0u;
		initialBatchSpawned_ = false;
		destroyedEventSent_ = false;
		active_ = false;
	}

	void Update(
		IPlayerRuntime* player,
		CharacterWorld& characters,
		float deltaTime,
		const std::function<void(const std::string&)>& onEvent)
	{
		(void)player;
		prompt_ = {};
		if (!active_) return;
		SpawnTargetIfNeeded(characters.GetActorWorld());
		if (!target_) return;

		const float safeDeltaTime = std::max(0.0f, deltaTime);
		elapsedSec_ += safeDeltaTime;
		retargetTimer_ -= safeDeltaTime;
		if (retargetTimer_ <= 0.0f)
		{
			RetargetEnemies(characters);
			retargetTimer_ = 0.25f;
		}

		if (target_->IsDead())
		{
			BuildPrompt();
			if (!destroyedEventSent_ && onEvent)
			{
				destroyedEventSent_ = true;
				onEvent(GetDestroyedEventId());
			}
			return;
		}

		spawnTimer_ -= safeDeltaTime;
		if (spawnTimer_ <= 0.0f)
		{
			SpawnReinforcementBatch(characters, !initialBatchSpawned_);
			initialBatchSpawned_ = true;
			spawnTimer_ = ResolveSpawnInterval();
		}
		BuildPrompt();
	}

	void UpdateShadowMatrix(const K4E::Matrix4x4&) {}
	bool IsActive() const { return active_; }
	Stage3DefenseTargetActor* GetTarget() const { return target_; }
	const PromptSnapshot& GetPromptSnapshot() const { return prompt_; }

private:
	void SpawnTargetIfNeeded(K4E::ActorWorld& actorWorld)
	{
		if (target_) return;
		auto& target = actorWorld.SpawnActor<Stage3DefenseTargetActor>();
		target.Configure(targetPosition_, targetMaxHealth_);
		target.Update(0.0f);
		target_ = &target;
	}

	void RetargetEnemies(CharacterWorld& characters)
	{
		if (!target_ || target_->IsDead()) return;
		for (EnemyBase* enemyBase : characters.GetEnemyRawList())
		{
			auto* enemy = dynamic_cast<K4E::EnemyActor*>(enemyBase);
			if (!enemy || enemy->IsDead()) continue;
			enemy->SetTargetActor(target_); // Wave生成済みと追加増援の両方を防衛コアへ継続的に向け直す。
		}
	}

	void SpawnReinforcementBatch(CharacterWorld& characters, bool forceInitialBatch)
	{
		if (!target_ || target_->IsDead()) return;
		const int phase = elapsedSec_ >= 60.0f ? 2 : (elapsedSec_ >= 30.0f ? 1 : 0);
		const int aliveCap = phase == 0 ? 12 : (phase == 1 ? 16 : 20);
		const int aliveCount = characters.GetAliveNormalEnemyCount();
		if (!forceInitialBatch && aliveCount >= aliveCap) return;

		const std::array<K4E::Vector3, 4> laneOffsets = {{
			{ 0.0f, 2.0f, 15.0f },
			{ -15.0f, 2.0f, 0.0f },
			{ 15.0f, 2.0f, 0.0f },
			{ 0.0f, 2.0f, -15.0f }
		}};
		const K4E::Vector3 laneCenter = targetPosition_ + laneOffsets[spawnSequence_ % laneOffsets.size()];
		const int requestedCount = forceInitialBatch ? 3 : (phase == 0 ? 2 : 3);
		const int spawnCount = forceInitialBatch ? requestedCount : std::min(requestedCount, std::max(0, aliveCap - aliveCount));

		for (int index = 0; index < spawnCount; ++index)
		{
			const float side = static_cast<float>(index) - static_cast<float>(spawnCount - 1) * 0.5f;
			const K4E::Vector3 spread{ side * 2.2f, 0.0f, static_cast<float>(index % 2) * 0.7f };
			const bool useMidRange = phase > 0 && index == spawnCount - 1;
			EnemyBase& spawned = characters.SpawnEnemyAt(laneCenter + spread, useMidRange ? EnemyType::MidRange : EnemyType::Melee);
			if (auto* enemy = dynamic_cast<K4E::EnemyActor*>(&spawned)) enemy->SetTargetActor(target_);
		}
		++spawnSequence_;
	}

	float ResolveSpawnInterval() const
	{
		if (elapsedSec_ >= 60.0f) return 4.5f;
		if (elapsedSec_ >= 30.0f) return 5.5f;
		return 7.0f;
	}

	void BuildPrompt()
	{
		if (!target_) return;
		char text[96]{};
		if (target_->IsDead())
		{
			prompt_.visible = true;
			prompt_.text = "防衛コアが破壊された";
			prompt_.normalizedProgress = 0.0f;
			return;
		}
		std::snprintf(text, sizeof(text), "防衛コア HP %.0f / %.0f", target_->GetHP(), target_->GetMaxHP());
		prompt_.visible = true;
		prompt_.text = text;
		prompt_.normalizedProgress = target_->GetHealthRatio();
	}

	Stage3DefenseTargetActor* target_ = nullptr;
	PromptSnapshot prompt_{};
	K4E::Vector3 targetPosition_{};
	float targetMaxHealth_ = 2200.0f;
	float elapsedSec_ = 0.0f;
	float spawnTimer_ = 1.5f;
	float retargetTimer_ = 0.0f;
	std::size_t spawnSequence_ = 0u;
	bool initialBatchSpawned_ = false;
	bool destroyedEventSent_ = false;
	bool active_ = false;
};
