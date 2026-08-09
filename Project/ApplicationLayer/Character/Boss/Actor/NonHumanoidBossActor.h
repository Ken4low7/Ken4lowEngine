#pragma once

#include "BossActorAttackComponent.h"
#include "BossBrainComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"

#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <SceneComponent.h>

namespace Ken4lowEngine
{
	/// 人型表示を前提にせず、Character共通機能とBoss専用の判断・攻撃・フェーズだけを束ねるActor。
	class NonHumanoidBossActor final : public CharacterActor
	{
	public:
		/// 非人型Bossに必要な専用Componentを不足分だけ生成し、共通Character Componentへ接続する。
		void Initialize() override
		{
			SceneComponent* root = GetRootComponent();
			if (!root)
			{
				root = &CreateRootComponent<SceneComponent>();
				root->SetName("Non Humanoid Boss Root");
				root->SetUpdateOrder(-100);
			}

			if (!GetBossPhaseComponent())
			{
				auto& phase = AddComponent<BossPhaseComponent>();
				phase.SetName("Boss Phase");
				phase.SetUpdateOrder(-98);
			}
			if (!GetBossBrainComponent())
			{
				auto& brain = AddComponent<BossBrainComponent>();
				brain.SetName("Boss Brain");
				brain.SetUpdateOrder(-95);
			}
			if (!GetBossAttackComponent())
			{
				auto& attack = AddComponent<BossAttackComponent>();
				attack.SetName("Boss Attack");
				attack.SetUpdateOrder(-92);
			}
			if (!GetComponent<RigidbodyComponent>())
			{
				auto& rigidbody = AddComponent<RigidbodyComponent>();
				rigidbody.SetName("Non Humanoid Boss Rigidbody");
				rigidbody.SetUpdateOrder(-85);
				rigidbody.SetBodyType(BodyType::Dynamic);
				rigidbody.SetMass(20.0f); // 非人型Bossも大型BodyとしてPlayerから押されにくい質量比にする。
				rigidbody.SetUseGravity(true);
				rigidbody.SetSleepEnabled(false);
				rigidbody.SetRestitution(0.0f);
				rigidbody.SetStaticFriction(0.0f);
				rigidbody.SetDynamicFriction(0.0f);
			}

			const bool hadHealth = GetHealthComponent() != nullptr;
			const bool hadCollider = GetColliderComponent() != nullptr;
			CharacterActor::Initialize();

			if (CharacterMovementComponent* movement = GetMovementComponent())
			{
				movement->SetMaxDriveForce(400.0f);
				movement->SetMaxBrakingForce(600.0f);
			}
			if (!hadHealth)
			{
				if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(1200.0f);
			}
			if (!hadCollider)
			{
				if (CharacterColliderComponent* collider = GetColliderComponent())
				{
					collider->SetHalfSize({ 2.0f, 2.0f, 2.0f });
					collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
				}
			}
		}

		/// JSON保存・復元とArchetype生成で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "NonHumanoidBossActor"; }

		/// BrainとAttackへ同じ追跡対象を設定する。
		void SetTargetActor(CharacterActor* targetActor)
		{
			if (BossBrainComponent* brain = GetBossBrainComponent()) brain->SetTargetActor(targetActor);
			if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetTargetActor(targetActor);
		}

		BossBrainComponent* GetBossBrainComponent() { return GetCharacterComponent<BossBrainComponent>(); }
		BossAttackComponent* GetBossAttackComponent() { return GetCharacterComponent<BossAttackComponent>(); }
		BossPhaseComponent* GetBossPhaseComponent() { return GetCharacterComponent<BossPhaseComponent>(); }

	protected:
		/// 死亡時は判断・攻撃・移動・Collider・物理速度を停止する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override
		{
			(void)deathEvent;
			if (BossBrainComponent* brain = GetBossBrainComponent()) brain->StopBehavior();
			if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetAttackEnabled(false);
			if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
			if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({});
			if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(false);
		}
	};
} // namespace Ken4lowEngine
