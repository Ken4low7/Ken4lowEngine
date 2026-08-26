#include "ActorWorld.h"

#include "ColliderComponent.h"
#include "RigidbodyComponent.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		Vector3 CalculateInertiaScale(const Collider* collider)
		{
			if (!collider)
			{
				return { 1.0f, 1.0f, 1.0f };
			}

			switch (collider->GetShapeType())
			{
			case ECollisionShapeType::AABB:
				{
					const AABB aabb = collider->GetAABB();
					const Vector3 half = (aabb.max - aabb.min) * 0.5f;
					return {
						std::max((half.y * half.y + half.z * half.z) / 3.0f, 0.0001f),
						std::max((half.x * half.x + half.z * half.z) / 3.0f, 0.0001f),
						std::max((half.x * half.x + half.y * half.y) / 3.0f, 0.0001f)
					};
				}
			case ECollisionShapeType::OBB:
				{
					const Vector3 half = collider->GetOBB().size;
					return {
						std::max((half.y * half.y + half.z * half.z) / 3.0f, 0.0001f),
						std::max((half.x * half.x + half.z * half.z) / 3.0f, 0.0001f),
						std::max((half.x * half.x + half.y * half.y) / 3.0f, 0.0001f)
					};
				}
			case ECollisionShapeType::Sphere:
				{
					const float radius = std::max(collider->GetSphere().radius, 0.001f);
					const float scale = std::max(0.4f * radius * radius, 0.0001f);
					return { scale, scale, scale };
				}
			case ECollisionShapeType::Capsule:
				{
					const Capsule capsule = collider->GetCapsule();
					const float effectiveRadius = std::max(capsule.radius + Vector3::Length(capsule.segment.diff) * 0.5f, 0.001f);
					const float scale = std::max(0.4f * effectiveRadius * effectiveRadius, 0.0001f);
					return { scale, scale, scale }; // W4ではCapsuleを包絡球近似し、極端な角加速度だけを避ける。
				}
			default:
				return { 1.0f, 1.0f, 1.0f };
			}
		}
	}

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

		// Phase 2以降は毎フレーム呼ばれないため、状態変更イベント時は必ず最新構成を同期する。
		auto colliders = actor.GetComponents<ColliderComponent>();
		auto* rigidbody = actor.GetComponent<RigidbodyComponent>();
		Rigidbody* physicsRigidbody = rigidbody && rigidbody->IsActiveInHierarchy() ? rigidbody->GetRigidbody() : nullptr;
		bool hasRegisteredPhysics = false;

		if (physicsRigidbody)
		{
			for (ColliderComponent* collider : colliders)
			{
				if (!collider || !collider->IsActiveInHierarchy() || !collider->GetCollider())
				{
					continue;
				}

				physicsRigidbody->SetInertiaScale(CalculateInertiaScale(collider->GetCollider()));
				break; // 1 Actorにつき主Collider 1個から慣性を決め、複数Colliderで上書きしない。
			}

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
