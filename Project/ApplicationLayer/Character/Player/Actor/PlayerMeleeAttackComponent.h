#pragma once

#include "PlayerCameraComponent.h"
#include "WeaponComponent.h"

#include "ApplicationLayer/Character/Boss/Actor/BossActor.h"
#include "CollisionManager.h"
#include "CollisionTypeIdDef.h"
#include "EnemyBase.h"
#include "EnemySpawnCrystal.h"

#include <Actor.h>
#include <ActorComponent.h>
#include <Camera.h>
#include <Segment.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの近接入力・予備動作・有効判定・硬直を管理し、Actor所有対象へ直接ダメージを与えるComponent。
	class PlayerMeleeAttackComponent final : public ActorComponent
	{
	public:
		void Update(float deltaTime) override
		{
			const float safeDeltaTime = (std::max)(0.0f, deltaTime);
			cooldownRemaining_ = (std::max)(0.0f, cooldownRemaining_ - safeDeltaTime);
			if (attackRequested_)
			{
				attackRequested_ = false;
				TryStartAttack();
			}
			if (!attacking_) return;
			attackTimer_ += safeDeltaTime;
			const float activeStart = startupDuration_;
			const float activeEnd = startupDuration_ + activeDuration_;
			if (!hitEvaluated_ && attackTimer_ >= activeStart && attackTimer_ <= activeEnd)
			{
				EvaluateHit();
				hitEvaluated_ = true; // 一度の近接攻撃で同じ対象へ多重ダメージを与えない。
			}
			if (attackTimer_ >= startupDuration_ + activeDuration_ + recoveryDuration_)
			{
				attacking_ = false;
				attackTimer_ = 0.0f;
				cooldownRemaining_ = cooldownDuration_;
			}
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("Player Melee");
			ImGui::Text("State: %s  Time: %.3f", attacking_ ? "Attacking" : "Ready", attackTimer_);
			ImGui::Text("Hit Count: %u", hitRevision_);
			ImGui::SliderFloat("Startup", &startupDuration_, 0.0f, 0.5f, "%.3f");
			ImGui::SliderFloat("Active", &activeDuration_, 0.01f, 0.5f, "%.3f");
			ImGui::SliderFloat("Recovery", &recoveryDuration_, 0.01f, 1.0f, "%.3f");
			ImGui::SliderFloat("Cooldown", &cooldownDuration_, 0.0f, 1.0f, "%.3f");
			ImGui::SliderFloat("Range", &range_, 0.5f, 8.0f, "%.2f");
			ImGui::SliderFloat("Radius", &radius_, 0.1f, 2.0f, "%.2f");
			ImGui::SliderInt("Damage", &damage_, 1, 100);
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerMeleeAttackComponent"; }
		void SetCollisionManager(::CollisionManager* collisionManager) { collisionManager_ = collisionManager; }
		void SetHitFeedbackCallback(std::function<void(bool)> callback) { hitFeedback_ = std::move(callback); }
		void RequestAttack() { attackRequested_ = true; }

		void ResetAttack()
		{
			attackRequested_ = false;
			attacking_ = false;
			hitEvaluated_ = false;
			attackTimer_ = 0.0f;
			cooldownRemaining_ = 0.0f;
		}

		bool IsAttacking() const { return attacking_; }
		bool CanStartAttack() const { return !attacking_ && cooldownRemaining_ <= 0.0f; }
		float GetNormalizedTime() const
		{
			const float total = startupDuration_ + activeDuration_ + recoveryDuration_;
			return total > 0.000001f ? std::clamp(attackTimer_ / total, 0.0f, 1.0f) : 0.0f;
		}
		unsigned int GetHitRevision() const { return hitRevision_; }

	private:
		void TryStartAttack()
		{
			if (!CanStartAttack()) return;
			Actor* owner = GetOwner();
			const WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			if (weapon && (weapon->IsReloading() || weapon->IsEquipAnimating())) return;
			attacking_ = true;
			hitEvaluated_ = false;
			attackTimer_ = 0.0f;
		}

		static float DistanceSquared(const Vector3& lhs, const Vector3& rhs)
		{
			const Vector3 delta = lhs - rhs;
			return delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
		}

		void EvaluateHit()
		{
			Actor* owner = GetOwner();
			const PlayerCameraComponent* cameraComponent = owner ? owner->GetComponent<PlayerCameraComponent>() : nullptr;
			Camera* camera = cameraComponent ? cameraComponent->GetCamera() : nullptr;
			if (!owner || !camera || !collisionManager_) return;

			Vector3 forward = Vector3::Normalize(camera->GetForward());
			if (Vector3::LengthSquared(forward) <= 0.000001f) forward = { 0.0f, 0.0f, 1.0f };
			Segment segment{};
			segment.origin = camera->GetTranslate() + forward * 0.35f;
			segment.diff = forward * range_;

			Collider* enemyHit = nullptr;
			Collider* bossHit = nullptr;
			Collider* crystalHit = nullptr;
			collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kEnemy), segment, &enemyHit);
			collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kBoss), segment, &bossHit);
			collisionManager_->SegmentCast(static_cast<uint32_t>(CollisionTypeIdDef::kCrystal), segment, &crystalHit);

			Collider* nearest = nullptr;
			float nearestDistanceSq = 0.0f;
			auto selectNearest = [&](Collider* candidate)
			{
				if (!candidate) return;
				const float distanceSq = DistanceSquared(segment.origin, candidate->GetCenterPosition());
				if (!nearest || distanceSq < nearestDistanceSq)
				{
					nearest = candidate;
					nearestDistanceSq = distanceSq;
				}
			};
			selectNearest(enemyHit);
			selectNearest(bossHit);
			selectNearest(crystalHit);
			if (!nearest) return;

			bool killed = false;
			const uint32_t type = nearest->GetTypeID();
			if (type == static_cast<uint32_t>(CollisionTypeIdDef::kEnemy))
			{
				EnemyBase* enemy = nearest->GetOwner<EnemyBase>();
				if (!enemy || enemy->IsDead()) return;
				const bool wasDead = enemy->IsDead();
				enemy->SpawnHitEffectAt(nearest->GetCenterPosition());
				enemy->TakeDamage(damage_, forward, knockbackPower_);
				killed = !wasDead && enemy->IsDead();
			}
			else if (type == static_cast<uint32_t>(CollisionTypeIdDef::kBoss))
			{
				BossActor* boss = nearest->GetOwner<BossActor>();
				if (!boss || boss->IsDead()) return;
				const bool wasDead = boss->IsDead();
				boss->ApplyBulletDamage(static_cast<float>(damage_), nearest->GetCenterPosition()); // 近接もBossActorの共通Health経路へ集約する。
				killed = !wasDead && boss->IsDead();
			}
			else if (type == static_cast<uint32_t>(CollisionTypeIdDef::kCrystal))
			{
				EnemySpawnCrystal* crystal = nearest->GetOwner<EnemySpawnCrystal>();
				if (!crystal) return;
				crystal->ApplyDamage(damage_);
			}
			else
			{
				return;
			}

			++hitRevision_;
			if (hitFeedback_) hitFeedback_(killed);
		}

	private:
		::CollisionManager* collisionManager_ = nullptr;
		std::function<void(bool)> hitFeedback_{};
		float startupDuration_ = 0.08f;
		float activeDuration_ = 0.10f;
		float recoveryDuration_ = 0.22f;
		float cooldownDuration_ = 0.18f;
		float attackTimer_ = 0.0f;
		float cooldownRemaining_ = 0.0f;
		float range_ = 3.2f;
		float radius_ = 0.75f;
		float knockbackPower_ = 2.0f;
		int damage_ = 35;
		unsigned int hitRevision_ = 0u;
		bool attackRequested_ = false;
		bool attacking_ = false;
		bool hitEvaluated_ = false;
	};
} // namespace Ken4lowEngine
