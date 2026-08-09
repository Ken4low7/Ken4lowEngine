#include "BossActor.h"

#include "BossActorAttackComponent.h"
#include "BossBrainComponent.h"
#include "BossPresentationComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossPhaseComponent.h"
#include "ApplicationLayer/Character/Boss/Components/BossWeakPointComponent.h"

#include <Collider.h>
#include <CollisionPreset.h>
#include <GameViewportConstants.h>
#include <GaugeComponent.h>
#include <PhysicsCollisionLayer.h>
#include <RigidbodyComponent.h>
#include <Scene/Actor/Character/CharacterAnimationComponent.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <Scene/Actor/Character/HumanoidVisualComponent.h>
#include <SceneComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	void BossActor::Initialize()
	{
		SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<SceneComponent>();
			root->SetName("Boss Root");
			root->SetUpdateOrder(-100);
		}

		auto* phase = GetBossPhaseComponent();
		if (!phase)
		{
			phase = &AddComponent<BossPhaseComponent>();
			phase->SetName("Boss Phase");
			phase->SetUpdateOrder(-98);
		}

		auto* presentation = GetBossPresentationComponent();
		if (!presentation)
		{
			presentation = &AddComponent<BossPresentationComponent>();
			presentation->SetName("Boss Presentation");
			presentation->SetUpdateOrder(-97);
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
			attack.SetUpdateOrder(-92); // Chargeの速度出力を同フレームの共通Movementより先に確定する。
		}

		if (!GetComponent<RigidbodyComponent>())
		{
			auto& rigidbody = AddComponent<RigidbodyComponent>();
			rigidbody.SetName("Boss Rigidbody");
			rigidbody.SetUpdateOrder(-85);
		}

		auto* visual = GetHumanoidVisualComponent();
		if (!visual)
		{
			visual = &AddComponent<HumanoidVisualComponent>();
			visual->SetName("Boss Humanoid Visual");
			visual->SetUpdateOrder(0);
			visual->SetDrawOrder(0);
			visual->SetCastShadowEnabled(true);
			visual->SetLocalScale({ 1.5f, 1.5f, 1.5f });
			visual->AttachTo(root);
		}

		if (!GetBossWeakPointComponent())
		{
			auto& weakPoint = AddComponent<BossWeakPointComponent>();
			weakPoint.SetName("Boss Weak Point");
			weakPoint.SetUpdateOrder(-40);
		}

		constexpr float screenWidth = static_cast<float>(GameViewportConstants::Width);
		constexpr float gaugeWidth = 700.0f;
		if (!GetHealthGaugeComponent())
		{
			auto& gauge = AddComponent<GaugeComponent>();
			gauge.SetName("Boss HP Gauge");
			gauge.SetDrawOrder(100);
			gauge.SetPosition({ (screenWidth - gaugeWidth) * 0.5f, 44.0f });
			gauge.SetSize({ gaugeWidth, 26.0f });
			gauge.SetBackgroundColor({ 0.05f, 0.05f, 0.05f, 0.88f });
			gauge.SetFillColor({ 0.78f, 0.16f, 0.16f, 1.0f });
			gauge.SetBorderColor({ 1.0f, 1.0f, 1.0f, 0.92f });
			gauge.SetBorderThickness(2.0f);
		}

		if (!GetHealthLabelComponent())
		{
			auto& label = AddComponent<TextComponent>();
			label.SetName("Boss HP Label");
			label.SetDrawOrder(101);
			label.SetText("BOSS HP");
			label.SetPosition({ screenWidth * 0.5f, 12.0f });
			label.SetAnchor({ 0.5f, 0.0f });
			label.SetFontSize(24.0f);
		}

		const bool hadHealth = GetHealthComponent() != nullptr;
		const bool hadCollider = GetColliderComponent() != nullptr;
		CharacterActor::Initialize();

		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->SetUpdateOrder(-90);
			movement->SetMaxDriveForce(400.0f);
			movement->SetMaxBrakingForce(600.0f); // 巨体でも攻撃停止時に必要以上に滑らない制動値を使う。
		}
		if (CharacterAnimationComponent* animation = GetAnimationComponent()) animation->SetUpdateOrder(-70);
		if (!hadHealth)
		{
			if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(1200.0f);
		}
		if (!hadCollider)
		{
			if (CharacterColliderComponent* collider = GetColliderComponent())
			{
				collider->SetHalfSize({ 1.5f, 3.0f, 1.5f });
				collider->SetCollisionLayer(PhysicsCollisionLayer::DynamicActor);
			}
		}
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>())
		{
			rigidbody->SetBodyType(BodyType::Dynamic);
			rigidbody->SetMass(20.0f);
			rigidbody->SetUseGravity(true);
			rigidbody->SetSleepEnabled(false);
			rigidbody->SetRestitution(0.0f);
			rigidbody->SetStaticFriction(0.0f);
			rigidbody->SetDynamicFriction(0.0f); // Prefabの古い質量値を本番Boss設定へ上書きする。
		}
		if (Collider* primitive = GetCollisionPrimitive())
		{
			ApplyCollisionPreset(*primitive, ECollisionPresetId::Boss);
			primitive->SetOwner<BossActor>(this); // Legacy弾・照準もColliderからActor正本へ解決する。
		}
		if (visual && visual->GetSkinTexturePath().empty()) visual->ApplySkinToAllParts("Characters/enemy.dds");

		hasDeathWorldPosition_ = false;
		SetTargetActor(targetActor_);
		SetBattleEnabled(true);
		SetHealthHudVisible(true);
		SyncHealthHud();
	}

	void BossActor::Update(float deltaTime)
	{
		CharacterActor::Update(deltaTime);
		if (IsDead()) RestoreDeathWorldPosition();
		SyncHealthHud(); // BossのHPはActor所有Gauge Componentへ同期する。
	}

	void BossActor::PostPhysicsUpdate(float deltaTime)
	{
		CharacterActor::PostPhysicsUpdate(deltaTime);
		if (IsDead()) RestoreDeathWorldPosition(); // 重力・押し戻し後も撃破地点で崩壊演出を続ける。
	}

	void BossActor::SetTargetActor(CharacterActor* targetActor)
	{
		targetActor_ = targetActor;
		if (BossBrainComponent* brain = GetBossBrainComponent()) brain->SetTargetActor(targetActor_);
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetTargetActor(targetActor_);
	}

	CharacterDamageResult BossActor::ApplyWeakPointDamage(std::string_view partId, float baseDamage)
	{
		if (BossWeakPointComponent* weakPoint = GetBossWeakPointComponent()) return weakPoint->ApplyDamageToPart(partId, baseDamage);
		return {};
	}

	CharacterDamageResult BossActor::ApplyBulletDamage(float damage, const Vector3& hitPosition)
	{
		CharacterDamageInfo damageInfo{};
		damageInfo.amount = damage;
		damageInfo.hitPosition = hitPosition;
		damageInfo.hasHitPosition = true;
		return ApplyDamage(damageInfo); // 部位Collider移行前はBody倍率として共通Healthへ適用する。
	}

	void BossActor::ResetForValidation(const Vector3& worldPosition)
	{
		SetActive(true);
		hasDeathWorldPosition_ = false;
		SetPosition(worldPosition);
		if (CharacterHealthComponent* health = GetHealthComponent()) health->ResetHealth(1200.0f);
		if (CharacterMovementComponent* movement = GetMovementComponent()) movement->Stop();
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({});
		if (BossPhaseComponent* phase = GetBossPhaseComponent()) phase->ResetPhase();
		if (BossPresentationComponent* presentation = GetBossPresentationComponent()) presentation->ResetPresentation();
		if (BossBrainComponent* brain = GetBossBrainComponent()) brain->ResetBehavior();
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->ResetAttackState();
		if (CharacterAnimationComponent* animation = GetAnimationComponent()) animation->Play("Idle", 1.5f, true);
		SetBattleEnabled(true);
		SetHealthHudVisible(true);
		SyncHealthHud();
	}

	void BossActor::SetPosition(const Vector3& worldPosition)
	{
		SceneComponent* root = GetRootComponent();
		if (!root) return;
		root->SetLocalPosition(worldPosition);
		root->RefreshWorldTransform();
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->Update(0.0f);
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>()) rigidbody->SetVelocity({});
	}

	Vector3 BossActor::GetPosition() const
	{
		const SceneComponent* root = GetRootComponent();
		return root ? root->GetWorldPosition() : Vector3{};
	}

	void BossActor::SetYaw(float yaw)
	{
		SceneComponent* root = GetRootComponent();
		if (!root) return;
		Vector3 rotation = root->GetLocalRotation();
		rotation.y = yaw;
		root->SetLocalRotation(rotation);
		root->RefreshWorldTransform();
	}

	void BossActor::ClearRootParentKeepingWorldPosition()
	{
		SceneComponent* root = GetRootComponent();
		if (!root || !root->GetParent()) return;
		const Vector3 worldPosition = root->GetWorldPosition();
		const Vector3 worldRotation = root->GetWorldRotation();
		const Vector3 worldScale = root->GetWorldScale();
		root->Detach();
		root->SetLocalPosition(worldPosition);
		root->SetLocalRotation(worldRotation);
		root->SetLocalScale(worldScale);
		root->RefreshWorldTransform(); // Intro用親Transformを外しても見た目のWorld座標を維持する。
	}

	void BossActor::ForceSyncWorldTransform()
	{
		if (SceneComponent* root = GetRootComponent()) root->RefreshWorldTransform();
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->Update(0.0f);
		if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->Update(0.0f);
	}

	bool BossActor::HasRootParent() const
	{
		const SceneComponent* root = GetRootComponent();
		return root && root->GetParent();
	}

	Vector3 BossActor::GetRootLocalPosition() const
	{
		const SceneComponent* root = GetRootComponent();
		return root ? root->GetLocalPosition() : Vector3{};
	}

	Vector3 BossActor::GetRootWorldPosition() const { return GetPosition(); }

	void BossActor::SetBattleEnabled(bool enabled)
	{
		battleEnabled_ = enabled && !IsDead();
		if (BossBrainComponent* brain = GetBossBrainComponent())
		{
			if (battleEnabled_) brain->ResetBehavior();
			else brain->StopBehavior();
		}
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetAttackEnabled(battleEnabled_);
		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->SetMovementEnabled(battleEnabled_);
			if (!battleEnabled_) movement->Stop();
		}
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(battleEnabled_);
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>())
		{
			rigidbody->SetUseGravity(battleEnabled_);
			if (!battleEnabled_) rigidbody->SetVelocity({});
			else rigidbody->WakeUp();
		}
	}

	void BossActor::SetHealthHudVisible(bool visible)
	{
		const bool shouldShow = visible && IsAlive();
		if (GaugeComponent* gauge = GetHealthGaugeComponent()) gauge->SetVisible(shouldShow);
		if (TextComponent* label = GetHealthLabelComponent()) label->SetVisible(shouldShow);
	}

	void BossActor::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		if (HumanoidVisualComponent* visual = GetHumanoidVisualComponent()) visual->UpdateShadowMatrices(lightViewProjection);
	}

	float BossActor::GetHP() const
	{
		const CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetCurrentHealth() : 0.0f;
	}

	float BossActor::GetMaxHP() const
	{
		const CharacterHealthComponent* health = GetHealthComponent();
		return health ? health->GetMaxHealth() : 0.0f;
	}

	int BossActor::GetCurrentPhase() const
	{
		const BossPhaseComponent* phase = GetBossPhaseComponent();
		return phase ? phase->GetCurrentPhase() : 1;
	}

	unsigned int BossActor::GetPhaseRevision() const
	{
		const BossPhaseComponent* phase = GetBossPhaseComponent();
		return phase ? phase->GetPhaseRevision() : 0u;
	}

	bool BossActor::IsDeathPresentationComplete() const
	{
		const BossPresentationComponent* presentation = GetBossPresentationComponent();
		return presentation && presentation->IsDeathPresentationComplete();
	}

	BossBrainComponent* BossActor::GetBossBrainComponent() { return GetCharacterComponent<BossBrainComponent>(); }
	const BossBrainComponent* BossActor::GetBossBrainComponent() const { return GetCharacterComponent<BossBrainComponent>(); }
	BossAttackComponent* BossActor::GetBossAttackComponent() { return GetCharacterComponent<BossAttackComponent>(); }
	const BossAttackComponent* BossActor::GetBossAttackComponent() const { return GetCharacterComponent<BossAttackComponent>(); }
	BossPhaseComponent* BossActor::GetBossPhaseComponent() { return GetCharacterComponent<BossPhaseComponent>(); }
	const BossPhaseComponent* BossActor::GetBossPhaseComponent() const { return GetCharacterComponent<BossPhaseComponent>(); }
	BossWeakPointComponent* BossActor::GetBossWeakPointComponent() { return GetCharacterComponent<BossWeakPointComponent>(); }
	const BossWeakPointComponent* BossActor::GetBossWeakPointComponent() const { return GetCharacterComponent<BossWeakPointComponent>(); }
	BossPresentationComponent* BossActor::GetBossPresentationComponent() { return GetCharacterComponent<BossPresentationComponent>(); }
	const BossPresentationComponent* BossActor::GetBossPresentationComponent() const { return GetCharacterComponent<BossPresentationComponent>(); }
	HumanoidVisualComponent* BossActor::GetHumanoidVisualComponent() { return GetCharacterComponent<HumanoidVisualComponent>(); }
	const HumanoidVisualComponent* BossActor::GetHumanoidVisualComponent() const { return GetCharacterComponent<HumanoidVisualComponent>(); }

	GaugeComponent* BossActor::GetHealthGaugeComponent()
	{
		for (GaugeComponent* gauge : GetComponents<GaugeComponent>()) if (gauge && gauge->GetName() == "Boss HP Gauge") return gauge;
		return nullptr;
	}

	const GaugeComponent* BossActor::GetHealthGaugeComponent() const
	{
		for (const GaugeComponent* gauge : GetComponents<GaugeComponent>()) if (gauge && gauge->GetName() == "Boss HP Gauge") return gauge;
		return nullptr;
	}

	TextComponent* BossActor::GetHealthLabelComponent()
	{
		for (TextComponent* label : GetComponents<TextComponent>()) if (label && label->GetName() == "Boss HP Label") return label;
		return nullptr;
	}

	const TextComponent* BossActor::GetHealthLabelComponent() const
	{
		for (const TextComponent* label : GetComponents<TextComponent>()) if (label && label->GetName() == "Boss HP Label") return label;
		return nullptr;
	}

	void BossActor::SyncHealthHud()
	{
		CharacterHealthComponent* health = GetHealthComponent();
		GaugeComponent* gauge = GetHealthGaugeComponent();
		if (!health || !gauge) return;
		gauge->SetMaxValue(health->GetMaxHealth());
		gauge->SetValue(health->GetCurrentHealth());
	}

	void BossActor::RestoreDeathWorldPosition()
	{
		if (!hasDeathWorldPosition_) return;
		SceneComponent* root = GetRootComponent();
		if (!root) return;
		root->SetLocalPosition(deathWorldPosition_);
		root->RefreshWorldTransform();
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->Update(0.0f);
	}

	void BossActor::OnDeath(const CharacterDeathEvent& deathEvent)
	{
		(void)deathEvent;
		deathWorldPosition_ = GetPosition();
		hasDeathWorldPosition_ = true;
		ClearRootParentKeepingWorldPosition();
		SetHealthHudVisible(false);
		battleEnabled_ = false;
		if (BossBrainComponent* brain = GetBossBrainComponent()) brain->StopBehavior();
		if (BossAttackComponent* attack = GetBossAttackComponent()) attack->SetAttackEnabled(false);
		if (CharacterMovementComponent* movement = GetMovementComponent())
		{
			movement->Stop();
			movement->SetMovementEnabled(false);
		}
		if (RigidbodyComponent* rigidbody = GetComponent<RigidbodyComponent>())
		{
			rigidbody->SetVelocity({});
			rigidbody->SetUseGravity(false); // 死亡演出中は撃破地点から物理で流されないよう固定する。
		}
		if (CharacterColliderComponent* collider = GetColliderComponent()) collider->SetActive(false);
		if (BossPresentationComponent* presentation = GetBossPresentationComponent()) presentation->StartDeathPresentation();
		RestoreDeathWorldPosition();
		SyncHealthHud();
	}
} // namespace Ken4lowEngine
