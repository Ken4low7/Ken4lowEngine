#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../Core/EnemyBase.h"
#include "../Core/EnemyType.h"

#include <AABB.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Ken4lowEngine
{
	class EnemyAIComponent;
	class EnemyAttackComponent;
	class EnemyEffectComponent;
	class MidRangeEnemyAIComponent;
	class MidRangeEnemyAttackComponent;
	class HumanoidVisualComponent;
	class WorldGaugeComponent;

	/// EnemyBase互換を維持しながら、近接・中距離敵のAIと攻撃をアーキタイプ別Componentへ委譲する本番Actor。
	class EnemyActor final : public ::EnemyBase
	{
	public:
		explicit EnemyActor(::EnemyType enemyType = ::EnemyType::Melee) : enemyType_(enemyType)
		{
			auto& rigidbody = AddComponent<RigidbodyComponent>();
			rigidbody.SetName("Enemy Rigidbody");
			rigidbody.SetUpdateOrder(-65);
			rigidbody.SetBodyType(BodyType::Dynamic);
			rigidbody.SetMass(4.0f);
			rigidbody.SetUseGravity(true);
			rigidbody.SetSleepEnabled(false);
			rigidbody.SetRestitution(0.0f);
			rigidbody.SetStaticFriction(0.2f);
			rigidbody.SetDynamicFriction(0.05f); // AIのMotor速度を床摩擦で毎Stepほぼ消さず、遮蔽物上でも移動を継続させる。
		}

		/// 必要なComponentを不足分だけ生成し、アーキタイプ別の基礎値を設定する。
		void Initialize() override;

		/// EnemyBaseの地形・死亡処理とComponent更新を進め、表示用状態を同期する。
		void Update(float deltaTime) override;

		/// Rigidbodyが補正したCollider中心をRootへ戻し、次フレームのVisual更新へ反映する。
		void PostPhysicsUpdate(float deltaTime) override
		{
			if (IsDead()) return;
			RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>();
			if (!rigidbody) return; // Rigidbodyを持たない比較用Actorは従来のEnemyBase解決だけを使う。

			useGravity_ = false;
			useWorldResolve_ = false;
			rigidbody->SetStaticFriction(0.2f);
			rigidbody->SetDynamicFriction(0.05f); // Prefab値が旧設定でもCharacter Motor向けの低摩擦へ毎フレーム同期する。
			EnsureAutomaticTraversalProfile();
			rigidbody->PostPhysicsUpdate(deltaTime);
			if (CharacterColliderComponent* collider = GetColliderComponent()) collider->PostPhysicsUpdate(deltaTime);
		}

		/// Difficulty Directorの倍率を現在アーキタイプのAIと攻撃Componentへ適用する。
		void ApplyDirectorDifficulty(float moveSpeedMultiplier, float attackCooldownMultiplier, float damageMultiplier) override;

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyActor"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		::EnemyType GetEnemyType() const { return enemyType_; }

		/// IntroなどPlayerだけを進める区間ではAIだけでなく重力を含むEnemy物理も停止する。
		void SetSimulationEnabled(bool enabled)
		{
			simulationEnabled_ = enabled;
			if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>())
			{
				rigidbody->SetUseGravity(enabled);
				if (!enabled) rigidbody->SetVelocity({});
				else rigidbody->WakeUp(); // 本編開始時は停止位置から重力・接地判定を再開する。
			}
		}
		bool IsSimulationEnabled() const { return simulationEnabled_; }

		/// 高いSpawn位置は維持して重力落下させ、床より低い位置だけ安全な接地高さへ補正する。
		void SetPosition(const Vector3& worldPosition)
		{
			Vector3 resolvedPosition = CorrectSpawnPosition(worldPosition);
			resolvedPosition.y = std::max(worldPosition.y, resolvedPosition.y);
			spawnPosition_ = resolvedPosition;
			lastSafePosition_ = resolvedPosition;
			SetCenterPosition(resolvedPosition);
			if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>())
			{
				useGravity_ = false;
				useWorldResolve_ = false;
				EnsureAutomaticTraversalProfile();
				rigidbody->SetUseGravity(simulationEnabled_);
				rigidbody->SetVelocity({}); // 再配置前の落下・移動速度を新しいSpawnへ持ち越さない。
				rigidbody->WakeUp();
			}
		}

		/// 各AIと攻撃Componentへ同じ追跡対象を設定する。Initialize前の指定も保持する。
		void SetTargetActor(CharacterActor* targetActor);

		/// A* ComponentへStage障害物を設定する。Initialize前の指定も保持し、所有権は移さない。
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);

		/// Debug比較用ダメージをEnemyBaseの本番Damage経路へ適用する。
		CharacterDamageResult ApplyComparisonDamage(float amount);

		/// DebugSceneで同じ個体を再比較できるよう生存・位置・各専用状態を戻す。
		void ResetForComparison(const Vector3& worldPosition);

		/// 照準中だけ頭上HP Gaugeを表示する。
		void SetHealthBarVisible(bool visible);

		/// 自爆Componentだけが無敵判定を迂回して共通死亡演出へ接続する。
		void KillAfterSuicide();

		EnemyAIComponent* GetEnemyAIComponent();
		const EnemyAIComponent* GetEnemyAIComponent() const;
		EnemyAttackComponent* GetEnemyAttackComponent();
		const EnemyAttackComponent* GetEnemyAttackComponent() const;
		MidRangeEnemyAIComponent* GetMidRangeEnemyAIComponent();
		const MidRangeEnemyAIComponent* GetMidRangeEnemyAIComponent() const;
		MidRangeEnemyAttackComponent* GetMidRangeEnemyAttackComponent();
		const MidRangeEnemyAttackComponent* GetMidRangeEnemyAttackComponent() const;
		EnemyEffectComponent* GetEnemyEffectComponent();
		const EnemyEffectComponent* GetEnemyEffectComponent() const;
		HumanoidVisualComponent* GetHumanoidVisualComponent();
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const;
		WorldGaugeComponent* GetHealthGaugeComponent();
		const WorldGaugeComponent* GetHealthGaugeComponent() const;

		void TakeDamage(int amount) override;
		void TakeDamage(int amount, const Vector3& hitDir, float hitPower) override;

	protected:
		/// 共通Healthが死亡へ遷移した時点で全AI・攻撃・移動を停止する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		void EnsureAutomaticTraversalProfile()
		{
			CharacterMovementComponent* movement = GetMovementComponent();
			if (!movement) return;
			const bool profileChanged = std::fabs(movement->GetAutomaticObstacleMaxClimbHeight() - 4.2f) > 0.01f;
			if (profileChanged)
			{
				movement->ConfigureAutomaticObstacleTraversal(true, 4.2f, 2.6f, 7.5f, 0.65f);
				SetNavigationObstacles(navigationObstacles_); // 4m級のPropを登れる設定へ変えた時だけA*の通過・迂回対象を再構築する。
			}
			movement->SetMovementEnabled(true);
		}

		/// CharacterHealthの現在値をWorldGaugeへ反映する。
		void SyncHealthGauge();
		void ApplyPendingRuntimeBindings();
		void RebuildNavigationBlockingObstacles();
		void EnsureRuntimeStateInitialized();
		void EnsureArchetypeComponents();
		int GetConfiguredArchetypeMaxHp() const;

	private:
		CharacterActor* targetActor_ = nullptr;
		const std::vector<AABB>* navigationObstacles_ = nullptr;
		std::vector<AABB> navigationBlockingObstacles_{};
		::EnemyType enemyType_ = ::EnemyType::Melee;
		bool runtimeStateInitialized_ = false;
		bool simulationEnabled_ = true;
	};
} // namespace Ken4lowEngine
