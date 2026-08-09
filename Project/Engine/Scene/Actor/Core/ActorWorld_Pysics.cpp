#include "ActorWorld.h"

#include "ColliderComponent.h"
#include "RigidbodyComponent.h"

namespace Ken4lowEngine
{
	void ActorWorld::SetPhysicsWorld(PhysicsWorld* physicsWorld)
	{
		if (physicsWorld_ == physicsWorld) return;

		if (physicsWorld_)
		{
			for (auto& actor : actors_)
			{
				if (actor) UnregisterPhysicsComponents(*actor); // 旧Worldへ外部参照を残してから差し替えない。
			}
		}

		physicsWorld_ = physicsWorld;
		SetupDefaultPhysicsSettings();

		if (physicsWorld_ && isInitialized_)
		{
			for (auto& actor : actors_)
			{
				if (actor) RegisterPhysicsComponents(*actor);
			}
		}
	}

	void ActorWorld::SetupDefaultPhysicsSettings()
	{
		if (!physicsWorld_)
		{
			return; // PhysicsWorldが設定されていない場合は何もしない
		}

		// 動的Actor同士は物理的に衝突させる。
		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::DynamicActor,
			PhysicsCollisionLayer::DynamicActor,
			CollisionResponseType::Block);

		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::DynamicActor,
			PhysicsCollisionLayer::WorldStatic,
			CollisionResponseType::Block); // 動くActorと床は物理的に衝突させる。

		// 静的ステージ同士は毎フレーム接触判定する必要がないためNarrowPhaseへ送らない。
		physicsWorld_->GetResponseMatrix().SetResponse(
			PhysicsCollisionLayer::WorldStatic,
			PhysicsCollisionLayer::WorldStatic,
			CollisionResponseType::Ignore);
	}

	void ActorWorld::RegisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_ || !actor.IsActive() || actor.IsPendingDestroy())
		{
			UnregisterPhysicsComponents(actor);
			return;
		}

		// 一度登録済みの静的World Actorは、417個規模のCollider登録確認を毎フレーム繰り返さない。
		if (actor.IsPhysicsRegistered() && actor.GetLayer() == "WorldStatic")
		{
			return;
		}

		auto colliders = actor.GetComponents<ColliderComponent>();
		auto* rigidbody = actor.GetComponent<RigidbodyComponent>();
		Rigidbody* physicsRigidbody = rigidbody && rigidbody->IsActiveInHierarchy() ? rigidbody->GetRigidbody() : nullptr;
		bool hasRegisteredPhysics = false;

		if (physicsRigidbody)
		{
			physicsWorld_->RegisterRigidbody(physicsRigidbody);
			hasRegisteredPhysics = true;
		}
		else if (rigidbody && rigidbody->GetRigidbody())
		{
			physicsWorld_->UnregisterRigidbody(rigidbody->GetRigidbody()); // Component無効化を次の物理Step前に反映する。
		}

		for (ColliderComponent* collider : colliders)
		{
			if (!collider || !collider->GetCollider())
			{
				continue; // Colliderがnullptrなら登録しない
			}

			Collider* physicsCollider = collider->GetCollider();
			if (!collider->IsActiveInHierarchy())
			{
				physicsWorld_->UnregisterCollider(physicsCollider);
				physicsCollider->SetRigidbody(nullptr);
				continue;
			}

			physicsCollider->SetRigidbody(physicsRigidbody);
			physicsWorld_->RegisterCollider(physicsCollider);
			hasRegisteredPhysics = true;
		}

		actor.SetPhysicsRegistered(hasRegisteredPhysics);
	}

	void ActorWorld::UnregisterPhysicsComponents(Actor& actor)
	{
		if (!physicsWorld_)
		{
			actor.SetPhysicsRegistered(false);
			return;
		}

		auto colliders = actor.GetComponents<ColliderComponent>();
		for (ColliderComponent* collider : colliders)
		{
			if (!collider || !collider->GetCollider())
			{
				continue; // Colliderがnullptrなら解除しない
			}

			physicsWorld_->UnregisterCollider(collider->GetCollider());
			collider->GetCollider()->SetRigidbody(nullptr); // Rigidbody破棄後にColliderが古い参照を保持しない。
		}

		if (auto* rigidbody = actor.GetComponent<RigidbodyComponent>())
		{
			// ActorのRigidbodyをPhysicsWorldから登録解除する
			physicsWorld_->UnregisterRigidbody(rigidbody->GetRigidbody());
		}

		actor.SetPhysicsRegistered(false);
	}
} // namespace Ken4lowEngine
