#include "CharacterActor.h"

#include "CharacterHealthComponent.h"
#include "CharacterMovementComponent.h"
#include "CharacterTargetComponent.h"
#include "CharacterColliderComponent.h"
#include "CharacterAnimationComponent.h"
#include "AttackComponent.h"
#include "SceneComponent.h"
#include <Collider.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	void CharacterActor::Initialize()
	{
		SceneComponent* root = GetRootComponent();
		if (!root)
		{
			root = &CreateRootComponent<SceneComponent>();
			root->SetName("Character Root");
			root->SetUpdateOrder(-100);
		}

		if (!GetHealthComponent())
		{
			CharacterHealthComponent& health = AddComponent<CharacterHealthComponent>();
			health.SetName("Character Health");
			health.SetUpdateOrder(-50);
		}

		if (!GetMovementComponent())
		{
			CharacterMovementComponent& movement = AddComponent<CharacterMovementComponent>();
			movement.SetName("Character Movement");
			movement.SetUpdateOrder(-90);
		}

		if (!GetTargetComponent())
		{
			CharacterTargetComponent& target = AddComponent<CharacterTargetComponent>();
			target.SetName("Character Target");
			target.SetLocalPosition({ 0.0f, 1.0f, 0.0f });
			target.SetUpdateOrder(-80);
			target.AttachTo(root);
		}

		if (!GetAnimationComponent())
		{
			CharacterAnimationComponent& animation = AddComponent<CharacterAnimationComponent>();
			animation.SetName("Character Animation");
			animation.SetUpdateOrder(-70);
		}

		if (!GetColliderComponent())
		{
			CharacterColliderComponent& collider = AddComponent<CharacterColliderComponent>();
			collider.SetName("Character Collider");
			collider.SetUpdateOrder(-60);
			collider.AttachTo(root);
		}

		Actor::Initialize();
		deathNotificationSent_ = !IsAlive(); // 初期JSONが死亡状態でも生成直後の死亡イベントは発行しない。
		lastAcceptedDamageInfo_ = {};
	}

	void CharacterActor::Finalize()
	{
		ClearDamageListeners();
		ClearDeathListeners();
		deathNotificationSent_ = false;
		lastAcceptedDamageInfo_ = {};
		Actor::Finalize();
	}

	CharacterDamageResult CharacterActor::ApplyDamage(const CharacterDamageInfo& damageInfo)
	{
		CharacterDamageResult result{};
		result.requestedDamage = damageInfo.amount;

		CharacterHealthComponent* health = GetHealthComponent();
		if (!health || !IsActive() || IsPendingDestroy()) return result;

		if (health->IsAlive()) deathNotificationSent_ = false; // Component側で復活した後は次の死亡通知を許可する。
		result = health->ApplyDamage(damageInfo);
		if (result.accepted && result.appliedDamage > 0.0f)
		{
			lastAcceptedDamageInfo_ = damageInfo;
			NotifyDamageAccepted(damageInfo, result); // 通常敵・Boss・環境Damageを同じ受理済み通知へ集約する。
		}
		if (result.killed && !deathNotificationSent_)
		{
			deathNotificationSent_ = true;
			NotifyDeath(damageInfo, result);
		}
		return result;
	}

	CharacterDamageResult CharacterActor::ApplyDamage(float amount)
	{
		CharacterDamageInfo damageInfo{};
		damageInfo.amount = amount;
		return ApplyDamage(damageInfo);
	}

	bool CharacterActor::IsAlive() const
	{
		const CharacterHealthComponent* health = GetHealthComponent();
		return health && health->IsAlive();
	}

	Vector3 CharacterActor::GetTargetPosition() const
	{
		if (const CharacterTargetComponent* target = GetTargetComponent()) return target->GetTargetPosition();
		if (const SceneComponent* root = GetRootComponent()) return root->GetWorldPosition();
		return {};
	}

	void CharacterActor::OnCollisionEnter(const CollisionHit& hit)
	{
		OnCollisionEnter(hit.other); // 詳細Hit未対応の派生Actorも旧Collider*経路で同じ処理を受け取れるようにする。
	}

	void CharacterActor::OnCollisionStay(const CollisionHit& hit)
	{
		OnCollisionStay(hit.other);
	}

	void CharacterActor::OnCollisionExit(const CollisionHit& hit)
	{
		OnCollisionExit(hit.other);
	}

	void CharacterActor::OnOverlapBegin(const CollisionHit& hit)
	{
		OnCollisionEnter(hit);
	}

	void CharacterActor::OnOverlapStay(const CollisionHit& hit)
	{
		OnCollisionStay(hit);
	}

	void CharacterActor::OnOverlapEnd(const CollisionHit& hit)
	{
		OnCollisionExit(hit);
	}

	CharacterHealthComponent* CharacterActor::GetHealthComponent()
	{
		return GetCharacterComponent<CharacterHealthComponent>();
	}

	const CharacterHealthComponent* CharacterActor::GetHealthComponent() const
	{
		return GetCharacterComponent<CharacterHealthComponent>();
	}

	CharacterTargetComponent* CharacterActor::GetTargetComponent()
	{
		return GetCharacterComponent<CharacterTargetComponent>();
	}

	const CharacterTargetComponent* CharacterActor::GetTargetComponent() const
	{
		return GetCharacterComponent<CharacterTargetComponent>();
	}

	CharacterMovementComponent* CharacterActor::GetMovementComponent()
	{
		return GetCharacterComponent<CharacterMovementComponent>();
	}

	const CharacterMovementComponent* CharacterActor::GetMovementComponent() const
	{
		return GetCharacterComponent<CharacterMovementComponent>();
	}

	CharacterColliderComponent* CharacterActor::GetColliderComponent()
	{
		return GetCharacterComponent<CharacterColliderComponent>();
	}

	const CharacterColliderComponent* CharacterActor::GetColliderComponent() const
	{
		return GetCharacterComponent<CharacterColliderComponent>();
	}

	Collider* CharacterActor::GetCollisionPrimitive()
	{
		CharacterColliderComponent* colliderComponent = GetColliderComponent();
		return colliderComponent ? colliderComponent->GetCollider() : nullptr;
	}

	const Collider* CharacterActor::GetCollisionPrimitive() const
	{
		const CharacterColliderComponent* colliderComponent = GetColliderComponent();
		return colliderComponent ? colliderComponent->GetCollider() : nullptr;
	}

	CharacterAnimationComponent* CharacterActor::GetAnimationComponent()
	{
		return GetCharacterComponent<CharacterAnimationComponent>();
	}

	const CharacterAnimationComponent* CharacterActor::GetAnimationComponent() const
	{
		return GetCharacterComponent<CharacterAnimationComponent>();
	}

	AttackComponent* CharacterActor::GetAttackComponent()
	{
		return GetCharacterComponent<AttackComponent>(); // 通常敵とBossActorが同じ取得APIを利用する。
	}

	const AttackComponent* CharacterActor::GetAttackComponent() const
	{
		return GetCharacterComponent<AttackComponent>();
	}

	CharacterActor::DamageListenerId CharacterActor::AddDamageListener(DamageListener listener)
	{
		if (!listener) return 0;
		const DamageListenerId listenerId = nextDamageListenerId_++;
		damageListeners_.push_back({ listenerId, std::move(listener) });
		return listenerId;
	}

	bool CharacterActor::RemoveDamageListener(DamageListenerId listenerId)
	{
		if (listenerId == 0) return false;
		const auto listenerIt = std::find_if(damageListeners_.begin(), damageListeners_.end(),
			[listenerId](const DamageListenerEntry& entry) { return entry.id == listenerId; });
		if (listenerIt == damageListeners_.end()) return false;
		damageListeners_.erase(listenerIt);
		return true;
	}

	bool CharacterActor::HasDamageListener(DamageListenerId listenerId) const
	{
		return listenerId != 0 && std::any_of(damageListeners_.begin(), damageListeners_.end(),
			[listenerId](const DamageListenerEntry& entry) { return entry.id == listenerId; });
	}

	void CharacterActor::ClearDamageListeners()
	{
		damageListeners_.clear();
	}

	CharacterActor::DeathListenerId CharacterActor::AddDeathListener(DeathListener listener)
	{
		if (!listener) return 0;
		const DeathListenerId listenerId = nextDeathListenerId_++;
		deathListeners_.push_back({ listenerId, std::move(listener) });
		return listenerId;
	}

	bool CharacterActor::RemoveDeathListener(DeathListenerId listenerId)
	{
		if (listenerId == 0) return false;
		const auto listenerIt = std::find_if(deathListeners_.begin(), deathListeners_.end(),
			[listenerId](const DeathListenerEntry& entry) { return entry.id == listenerId; });
		if (listenerIt == deathListeners_.end()) return false;
		deathListeners_.erase(listenerIt);
		return true;
	}

	bool CharacterActor::HasDeathListener(DeathListenerId listenerId) const
	{
		return listenerId != 0 && std::any_of(deathListeners_.begin(), deathListeners_.end(),
			[listenerId](const DeathListenerEntry& entry) { return entry.id == listenerId; });
	}

	void CharacterActor::ClearDeathListeners()
	{
		deathListeners_.clear();
	}

	void CharacterActor::NotifyDamageAccepted(const CharacterDamageInfo& damageInfo, const CharacterDamageResult& damageResult)
	{
		std::vector<DamageListener> listeners;
		listeners.reserve(damageListeners_.size());
		for (const DamageListenerEntry& entry : damageListeners_)
		{
			if (entry.listener) listeners.push_back(entry.listener); // 通知中にListenerが自身を解除しても走査を壊さない。
		}
		for (const DamageListener& listener : listeners) listener(damageInfo, damageResult);
	}

	void CharacterActor::NotifyDeath(const CharacterDamageInfo& damageInfo, const CharacterDamageResult& damageResult)
	{
		CharacterDeathEvent deathEvent{};
		deathEvent.character = this;
		deathEvent.damage = damageInfo;
		deathEvent.result = damageResult;
		OnDeath(deathEvent);

		std::vector<DeathListener> listeners;
		listeners.reserve(deathListeners_.size());
		for (const DeathListenerEntry& entry : deathListeners_)
		{
			if (entry.listener) listeners.push_back(entry.listener); // 登録順を維持し、通知中の登録解除にも安全なコピーを使う。
		}
		for (const DeathListener& listener : listeners) listener(deathEvent);
	}
} // namespace Ken4lowEngine
