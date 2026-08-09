#include "EnemyActor.h"

#include "EnemyAIComponent.h"
#include "EnemyAttackComponent.h"
#include "EnemyEffectComponent.h"
#include "MidRangeEnemyComponents.h"

#include <Collider.h>
#include <CollisionPreset.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>
#include <WorldGaugeComponent.h>

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	namespace
	{
		const char* ToEnemyTypeString(::EnemyType enemyType)
		{
			return enemyType == ::EnemyType::MidRange ? "MidRange" : "Melee";
		}

		::EnemyType EnemyTypeFromString(const std::string& value)
		{
			return value == "MidRange" ? ::EnemyType::MidRange : ::EnemyType::Melee;
		}
	}

	void EnemyActor::Initialize()
	{
		SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<SceneComponent>();
			root->SetName("Enemy Root");
			root->SetUpdateOrder(-100);
		}

		EnsureArchetypeComponents();

		HumanoidVisualComponent* visual = GetHumanoidVisualComponent();
		if (visual)
		{
			visual->SetName("Enemy Humanoid Visual");
			visual->SetUpdateOrder(0);
			visual->SetDrawOrder(0);
			visual->SetCastShadowEnabled(true);
			visual->AttachTo(root);
		}

		if (!GetEnemyEffectComponent())
		{
			auto& effect = AddComponent<EnemyEffectComponent>();
			effect.SetName("Enemy Effect");
			effect.SetUpdateOrder(20);
		}

		if (!GetHealthGaugeComponent())
		{
			auto& healthGauge = AddComponent<WorldGaugeComponent>();
			healthGauge.SetName("Enemy HP Gauge");
			healthGauge.SetDrawOrder(120);
			healthGauge.SetLocalPosition({ 0.0f, 2.65f, 0.0f });
			healthGauge.SetSize({ 190.0f, 16.0f });
			healthGauge.SetScreenOffset({ 0.0f, -8.0f });
			healthGauge.SetBackgroundColor({ 0.05f, 0.05f, 0.05f, 0.86f });
			healthGauge.SetFillColor(enemyType_ == ::EnemyType::MidRange ? Vector4{ 0.25f, 0.68f, 0.95f, 1.0f } : Vector4{ 0.90f, 0.58f, 0.16f, 1.0f });
			healthGauge.SetBorderColor({ 1.0f, 1.0f, 1.0f, 0.90f });
			healthGauge.SetBorderThickness(2.0f);
			healthGauge.SetVisible(false);
			healthGauge.AttachTo(root);
		}

		SetMaxHp(GetConfiguredArchetypeMaxHp());
		EnemyBase::Initialize(); // Collision、地形補正、被弾、死亡演出は両アーキタイプで同じ本番経路を使う。
		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->ConfigureAutomaticObstacleTraversal(true, 2.8f, 2.4f, 6.5f, 0.55f);
			movement->SetMovementEnabled(false); // 通常敵だけに低障害物の乗越を有効化し、PlayerとBossのMovement設定は変更しない。
		}
		runtimeStateInitialized_ = true;
		simulationEnabled_ = true;
		if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/enemy.dds");
		ApplyPendingRuntimeBindings();
		SetHealthBarVisible(false);
		SyncHealthGauge();
	}

	void EnemyActor::Update(float deltaTime)
	{
		EnsureRuntimeStateInitialized();
		if (!simulationEnabled_)
		{
			SyncHealthGauge();
			return; // Intro・装備演出中はActorWorldに残したままEnemy固有時間だけを停止する。
		}
		if (IsDead())
		{
			if (MidRangeEnemyAttackComponent* attack = GetMidRangeEnemyAttackComponent()) attack->Update(deltaTime); // 所有者死亡後も発射済みBombの飛翔・爆発・Damageだけを最後まで更新する。
		}
		if (!GetComponent<RigidbodyComponent>())
		{
			if (const CharacterMovementComponent* movement = GetMovementComponent())
			{
				const Vector3 desiredVelocity = movement->GetVelocity();
				velocity_.x = desiredVelocity.x;
				velocity_.z = desiredVelocity.z; // Componentの要求速度をEnemyBaseの地形解決へ渡し、Root直接移動との二重適用を避ける。
			}
		}
		EnemyBase::Update(deltaTime);
		if (!IsDead())
		{
			if (const SceneComponent* root = GetRootComponent()) orientation_ = root->GetWorldRotation();
		}
		SyncHealthGauge();
	}

	void EnemyActor::ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier)
	{
		EnsureRuntimeStateInitialized();
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->ApplyMoveSpeedMultiplier(moveSpeedMultiplier);
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->ApplyDifficultyMultipliers(attackCooldownMultiplier, damageMultiplier);
		if (MidRangeEnemyAIComponent* ai = GetMidRangeEnemyAIComponent()) ai->ApplyMoveSpeedMultiplier(moveSpeedMultiplier);
		if (MidRangeEnemyAttackComponent* attack = GetMidRangeEnemyAttackComponent()) attack->ApplyDifficultyMultipliers(attackCooldownMultiplier, damageMultiplier);
	}

	void EnemyActor::ToJson(nlohmann::json& outJson) const
	{
		EnemyBase::ToJson(outJson);
		outJson["EnemyType"] = ToEnemyTypeString(enemyType_);
	}

	void EnemyActor::FromJson(const nlohmann::json& inJson)
	{
		EnemyBase::FromJson(inJson);
		enemyType_ = EnemyTypeFromString(inJson.value("EnemyType", std::string("Melee"))); // 旧Prefabに種別が無い場合も別アーキタイプの状態を持ち越さず近接へ戻す。
		runtimeStateInitialized_ = false; // Prefab再読込後はHP・Collision・Component接続を現在値から再構築する。
	}

	void EnemyActor::SetTargetActor(CharacterActor* targetActor)
	{
		targetActor_ = targetActor;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->SetTargetActor(targetActor_);
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->SetTargetActor(targetActor_);
		if (MidRangeEnemyAIComponent* ai = GetMidRangeEnemyAIComponent()) ai->SetTargetActor(targetActor_);
		if (MidRangeEnemyAttackComponent* attack = GetMidRangeEnemyAttackComponent()) attack->SetTargetActor(targetActor_);
	}

	void EnemyActor::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		navigationObstacles_ = obstacles;
		RebuildNavigationBlockingObstacles();
		const std::vector<AABB>* blockingObstacles = navigationObstacles_ ? &navigationBlockingObstacles_ : nullptr;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->SetNavigationObstacles(blockingObstacles);
		if (MidRangeEnemyAIComponent* ai = GetMidRangeEnemyAIComponent()) ai->SetNavigationObstacles(blockingObstacles);
	}

	CharacterDamageResult EnemyActor::ApplyComparisonDamage(float amount)
	{
		EnsureRuntimeStateInitialized();
		CharacterDamageResult result{};
		result.requestedDamage = amount;
		result.healthBefore = static_cast<float>(GetHp());
		const bool wasDead = IsDead();
		if (std::isfinite(amount) && amount > 0.0f)
		{
			TakeDamage(std::max(1, static_cast<int>(std::round(amount)))); // Debug比較も本番のEnemyBase Damage経路だけを使用する。
		}
		result.healthAfter = static_cast<float>(GetHp());
		result.appliedDamage = std::max(0.0f, result.healthBefore - result.healthAfter);
		result.accepted = result.appliedDamage > 0.0f;
		result.killed = !wasDead && IsDead();
		if (result.accepted)
		{
			if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->TriggerHitEffect(GetCenterPosition());
		}
		SyncHealthGauge();
		return result;
	}

	void EnemyActor::ResetForComparison(const Vector3& worldPosition)
	{
		EnsureRuntimeStateInitialized();
		SetActive(true);
		isDead_ = false;
		removable_ = false;
		deathBreakActive_ = false;
		deathBreakInitialized_ = false;
		hasDeathEffectOrigin_ = false;
		hasDeathPartWorldTransforms_ = false;
		deathTimer_ = 0.0f;
		deathPieces_.clear();
		velocity_ = {};
		orientation_ = {};
		hitFlashTimer_ = 0.0f;
		simulationEnabled_ = true;
		SetPosition(worldPosition);
		SetCurrentHp(GetConfiguredArchetypeMaxHp());
		SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		SetAllPartsActive(true);
		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->Stop();
			movement->SetMovementEnabled(GetComponent<RigidbodyComponent>() != nullptr);
		}
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(true);
		if (Collider* primitive = GetCollisionPrimitive()) primitive->SetEnabled(true);
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->ResetBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent())
		{
			attack->SetAttackEnabled(true);
			attack->ResetAttackState();
		}
		if (MidRangeEnemyAIComponent* ai = GetMidRangeEnemyAIComponent()) ai->ResetBehavior();
		if (MidRangeEnemyAttackComponent* attack = GetMidRangeEnemyAttackComponent()) attack->ResetAttackState();
		if (EnemyEffectComponent* effect = GetEnemyEffectComponent()) effect->ResetEffectState();
		ApplyPendingRuntimeBindings();
		SetHealthBarVisible(false);
		SyncHealthGauge();
	}

	void EnemyActor::SetHealthBarVisible(bool visible)
	{
		if (WorldGaugeComponent* gauge = GetHealthGaugeComponent()) gauge->SetVisible(visible && !IsDead());
	}

	void EnemyActor::KillAfterSuicide()
	{
		const Vector3 direction = targetActor_ ? Vector3::NormalizeSafe(GetCenterPosition() - targetActor_->GetTargetPosition(), { 0.0f, 1.0f, 0.0f }) : Vector3{ 0.0f, 1.0f, 0.0f };
		EnemyBase::TakeDamage(std::max(1, GetHp()), direction, 2.5f); // 自爆無敵を迂回し、既存の部位爆散と削除タイマーへ接続する。
	}

	EnemyAIComponent* EnemyActor::GetEnemyAIComponent() { return GetCharacterComponent<EnemyAIComponent>(); }
	const EnemyAIComponent* EnemyActor::GetEnemyAIComponent() const { return GetCharacterComponent<EnemyAIComponent>(); }
	EnemyAttackComponent* EnemyActor::GetEnemyAttackComponent() { return GetCharacterComponent<EnemyAttackComponent>(); }
	const EnemyAttackComponent* EnemyActor::GetEnemyAttackComponent() const { return GetCharacterComponent<EnemyAttackComponent>(); }
	MidRangeEnemyAIComponent* EnemyActor::GetMidRangeEnemyAIComponent() { return GetCharacterComponent<MidRangeEnemyAIComponent>(); }
	const MidRangeEnemyAIComponent* EnemyActor::GetMidRangeEnemyAIComponent() const { return GetCharacterComponent<MidRangeEnemyAIComponent>(); }
	MidRangeEnemyAttackComponent* EnemyActor::GetMidRangeEnemyAttackComponent() { return GetCharacterComponent<MidRangeEnemyAttackComponent>(); }
	const MidRangeEnemyAttackComponent* EnemyActor::GetMidRangeEnemyAttackComponent() const { return GetCharacterComponent<MidRangeEnemyAttackComponent>(); }
	EnemyEffectComponent* EnemyActor::GetEnemyEffectComponent() { return GetCharacterComponent<EnemyEffectComponent>(); }
	const EnemyEffectComponent* EnemyActor::GetEnemyEffectComponent() const { return GetCharacterComponent<EnemyEffectComponent>(); }
	HumanoidVisualComponent* EnemyActor::GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }
	const HumanoidVisualComponent* EnemyActor::GetHumanoidVisualComponent() const { return GetCharacterComponent<HumanoidVisualComponent>(); }

	WorldGaugeComponent* EnemyActor::GetHealthGaugeComponent()
	{
		for (WorldGaugeComponent* gauge : GetComponents<WorldGaugeComponent>())
		{
			if (gauge && gauge->GetName() == "Enemy HP Gauge") return gauge;
		}
		return nullptr;
	}

	const WorldGaugeComponent* EnemyActor::GetHealthGaugeComponent() const
	{
		for (const WorldGaugeComponent* gauge : GetComponents<WorldGaugeComponent>())
		{
			if (gauge && gauge->GetName() == "Enemy HP Gauge") return gauge;
		}
		return nullptr;
	}

	void EnemyActor::TakeDamage(int amount)
	{
		TakeDamage(amount, {}, 50.0f);
	}

	void EnemyActor::TakeDamage(int amount, const Vector3& hitDir, float hitPower)
	{
		if (const MidRangeEnemyAttackComponent* attack = GetMidRangeEnemyAttackComponent(); attack && attack->IsSuicideInvulnerable()) return;
		EnemyBase::TakeDamage(amount, hitDir, hitPower);
	}

	void EnemyActor::SyncHealthGauge()
	{
		CharacterHealthComponent* health = GetHealthComponent();
		WorldGaugeComponent* gauge = GetHealthGaugeComponent();
		if (!health || !gauge) return;
		gauge->SetMaxValue(health->GetMaxHealth());
		gauge->SetValue(health->GetCurrentHealth());
	}

	void EnemyActor::ApplyPendingRuntimeBindings()
	{
		SetTargetActor(targetActor_);
		SetNavigationObstacles(navigationObstacles_);
	}

	void EnemyActor::RebuildNavigationBlockingObstacles()
	{
		navigationBlockingObstacles_.clear();
		if (!navigationObstacles_) return;

		float maxClimbHeight = 2.8f;
		if (const CharacterMovementComponent* movement = GetMovementComponent()) maxClimbHeight = movement->GetAutomaticObstacleMaxClimbHeight();
		navigationBlockingObstacles_.reserve(navigationObstacles_->size());
		for (const AABB& obstacle : *navigationObstacles_)
		{
			const float obstacleHeight = std::max(0.0f, obstacle.max.y - obstacle.min.y);
			if (obstacleHeight <= maxClimbHeight + 0.05f) continue;
			navigationBlockingObstacles_.push_back(obstacle);
		}
		// 低いColliderは物理判定へ残し、A*だけが通過候補として扱うことで乗越と迂回を役割分担する。
	}

	void EnemyActor::EnsureRuntimeStateInitialized()
	{
		if (runtimeStateInitialized_) return;
		EnsureArchetypeComponents();
		InitializeComponents();
		if (CharacterHealthComponent* health = GetHealthComponent())
		{
			configuredMaxHp_ = std::max(1, static_cast<int>(std::round(health->GetMaxHealth())));
			isDead_ = !health->IsAlive();
		}
		else
		{
			configuredMaxHp_ = GetConfiguredArchetypeMaxHp();
			isDead_ = false;
		}

		removable_ = false;
		deathBreakActive_ = false;
		deathBreakInitialized_ = false;
		hasDeathEffectOrigin_ = false;
		hasDeathPartWorldTransforms_ = false;
		deathTimer_ = 0.0f;
		deathPieces_.clear();
		hitFlashTimer_ = 0.0f;
		velocity_ = {};
		const bool usesPhysicsBody = GetComponent<RigidbodyComponent>() != nullptr;
		useGravity_ = !usesPhysicsBody;
		useWorldResolve_ = !usesPhysicsBody;
		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->ConfigureAutomaticObstacleTraversal(true, 2.8f, 2.4f, 6.5f, 0.55f);
			movement->SetMovementEnabled(usesPhysicsBody);
		}
		if (CharacterColliderComponent* collider = GetColliderComponent())
		{
			obbHalf_ = collider->GetHalfSize();
			worldCol_.half = obbHalf_;
			worldCol_.centerOffset = collider->GetLocalPosition();
			worldColOverride_ = true;
		}
		ApplyCollisionPreset(*this, ECollisionPresetId::Enemy);
		if (Collider* primitive = GetCollisionPrimitive()) primitive->SetOwner(this);
		spawnPosition_ = GetCenterPosition();
		lastSafePosition_ = spawnPosition_;
		orientation_ = GetOrientation();
		SetColor(baseColor_);
		ApplyPendingRuntimeBindings();
		runtimeStateInitialized_ = true; // Prefab生成ではActor固有Initializeが呼ばれないため、最初の利用時に一度だけ補完する。
		SyncHealthGauge();
	}

	void EnemyActor::EnsureArchetypeComponents()
	{
		if (enemyType_ == ::EnemyType::MidRange)
		{
			if (!GetMidRangeEnemyAIComponent())
			{
				auto& ai = AddComponent<MidRangeEnemyAIComponent>();
				ai.SetName("MidRange Enemy AI");
				ai.SetUpdateOrder(-95);
			}
			if (!GetMidRangeEnemyAttackComponent())
			{
				auto& attack = AddComponent<MidRangeEnemyAttackComponent>();
				attack.SetName("MidRange Enemy Attack");
				attack.SetUpdateOrder(-80);
				attack.SetDrawOrder(10);
			}
			return;
		}

		if (!GetEnemyAIComponent())
		{
			auto& ai = AddComponent<EnemyAIComponent>();
			ai.SetName("Enemy AI");
			ai.SetUpdateOrder(-95);
		}
		if (!GetEnemyAttackComponent())
		{
			auto& attack = AddComponent<EnemyAttackComponent>();
			attack.SetName("Enemy Attack");
			attack.SetUpdateOrder(-80);
		}
	}

	int EnemyActor::GetConfiguredArchetypeMaxHp() const
	{
		return enemyType_ == ::EnemyType::MidRange ? 120 : 160;
	}

	void EnemyActor::OnDeath(const CharacterDeathEvent& deathEvent)
	{
		(void)deathEvent;
		if (EnemyAIComponent* ai = GetEnemyAIComponent()) ai->StopBehavior();
		if (EnemyAttackComponent* attack = GetEnemyAttackComponent()) attack->StopAttacking();
		if (MidRangeEnemyAIComponent* ai = GetMidRangeEnemyAIComponent()) ai->StopBehavior();
		if (MidRangeEnemyAttackComponent* attack = GetMidRangeEnemyAttackComponent()) attack->StopAttacking();
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		SetHealthBarVisible(false);
		SyncHealthGauge(); // Collider停止と部位死亡演出は直後のEnemyBase::TakeDamageへ一元化する。
	}
} // namespace Ken4lowEngine
