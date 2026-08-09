#include "StagePhysicsBinder.h"

#include "Collider.h"
#include "PhysicsWorld.h"

#include <algorithm>

namespace Ken4lowEngine
{
	StagePhysicsBinder::~StagePhysicsBinder()
	{
		// Binder破棄時にも解除し、Scene終了時の解除漏れを防ぐ。
		Unbind();
	}

	void StagePhysicsBinder::Bind(PhysicsWorld& physicsWorld, const std::vector<Collider*>& stageColliders)
	{
		// Stage側のStatic ColliderをPhysicsWorldへ登録する。所有権はStage側に残す。
		Unbind();
		physicsWorld_ = &physicsWorld;

		for (Collider* collider : stageColliders)
		{
			AddUniqueCollider(collider);
		}

		for (Collider* collider : boundColliders_)
		{
			physicsWorld_->RegisterCollider(collider);
		}
	}

	void StagePhysicsBinder::Unbind()
	{
		// Scene終了時の破棄済みCollider参照を防ぐため、PhysicsWorldから登録済みColliderを取り除く。
		if (physicsWorld_)
		{
			for (Collider* collider : boundColliders_)
			{
				physicsWorld_->UnregisterCollider(collider);
			}
		}

		boundColliders_.clear();
		physicsWorld_ = nullptr;
	}

	void StagePhysicsBinder::AddUniqueCollider(Collider* collider)
	{
		// nullptrや同一Colliderの二重登録を避け、PhysicsWorld側の登録数確認を分かりやすくする。
		if (!collider || std::find(boundColliders_.begin(), boundColliders_.end(), collider) != boundColliders_.end())
		{
			return;
		}

		boundColliders_.push_back(collider);
	}

} // namespace Ken4lowEngine
