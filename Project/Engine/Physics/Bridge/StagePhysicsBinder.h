#pragma once
#include <cstddef>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	class PhysicsWorld;

	/// -------------------------------------------------------------
	///                         Stage物理登録バインダー
	/// -------------------------------------------------------------
	class StagePhysicsBinder
	{
	public:
		StagePhysicsBinder() = default;
		~StagePhysicsBinder();

		StagePhysicsBinder(const StagePhysicsBinder&) = delete;
		StagePhysicsBinder& operator=(const StagePhysicsBinder&) = delete;

		// Stage側のStatic ColliderをPhysicsWorldへ登録する。
		void Bind(PhysicsWorld& physicsWorld, const std::vector<Collider*>& stageColliders);

		// Scene終了時の破棄済みCollider参照を防ぐため、登録済みStage Colliderを解除する。
		void Unbind();

		// 現在PhysicsWorldへStage Colliderを登録しているかを返す。
		bool IsBound() const { return physicsWorld_ != nullptr; }

		// 登録済みStage Collider数を返す。
		size_t GetBoundColliderCount() const { return boundColliders_.size(); }

	private:
		// StageCollider群をPhysicsWorldへ登録する橋渡し役として、重複しない登録リストを作る。
		void AddUniqueCollider(Collider* collider);

	private:
		PhysicsWorld* physicsWorld_ = nullptr; // 登録先PhysicsWorld。所有権は持たない。
		std::vector<Collider*> boundColliders_{}; // 登録済みStage Collider。所有権は持たない。
	};

} // namespace Ken4lowEngine
