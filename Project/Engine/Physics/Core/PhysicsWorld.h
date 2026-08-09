#pragma once
#include "Contact.h"
#include "PhysicsWorldSettings.h"
#include "../Collision/Core/CollisionResponseMatrix.h"
#include "PhysicsEventDispatcher.h"

#include <cstddef>
#include <vector>

namespace Ken4lowEngine
{
	class Collider;
	class Rigidbody;

	/// -------------------------------------------------------------
	///                         物理ワールド
	/// -------------------------------------------------------------
	class PhysicsWorld
	{
	public: /// ---------- メンバ関数 ---------- ///

		// Colliderを物理ワールドへ登録する。
		void RegisterCollider(Collider* collider);

		// Colliderを物理ワールドから登録解除する。
		void UnregisterCollider(Collider* collider);

		// 登録済みColliderをまとめて解除する。
		void ClearColliders();

		// Rigidbodyを物理ワールドへ登録する。
		void RegisterRigidbody(Rigidbody* rigidbody);

		// Rigidbodyを物理ワールドから登録解除する。
		void UnregisterRigidbody(Rigidbody* rigidbody);

		// 1ステップ分の物理更新を実行する。
		void Step(float deltaTime);

		// 固定更新用の入口として物理更新を進める。
		void Update(float deltaTime);

		// 登録済みRigidbodyの速度積分を行う。
		void IntegrateBodies(float deltaTime);

		// Rigidbodyの速度をCollider位置へ反映する
		void IntegrateColliderPositions(float deltaTime);

		// 登録済みCollider同士の接触を検出する。
		void DetectCollisions();

		// 検出済みContactを解決する。
		void ResolveContacts();

		// 外部設定をPhysicsWorldへ反映し、固定更新やSolver設定を調整する。
		void ApplySettings(const PhysicsWorldSettings& settings);

		// 現在のPhysicsWorld設定を取得する。
		PhysicsWorldSettings GetSettings() const;

		// 直近ステップのContact一覧を取得する。
		const std::vector<Contact>& GetContacts() const { return contacts_; }

		// 登録済みCollider一覧を取得する。Debug表示専用に参照を返し、所有権は渡さない。
		const std::vector<Collider*>& GetColliders() const { return colliders_; }

		// 登録済みRigidbody一覧を取得する。Debug表示専用に参照を返し、所有権は渡さない。
		const std::vector<Rigidbody*>& GetRigidbodies() const { return rigidbodies_; }

		// 登録済みCollider数を取得する。
		size_t GetColliderCount() const { return colliders_.size(); }

		// 直近ステップのContact数を取得する。
		size_t GetContactCount() const { return contacts_.size(); }

		// 直近ステップで生成された物理イベント一覧を取得する。
		const std::vector<PhysicsEvent>& GetEvents() const { return eventDispatcher_.GetEvents(); }

		// PhysicsWorldのイベント通知先を追加する。
		void AddPhysicsEventListener(IPhysicsEventListener* listener) { eventDispatcher_.AddListener(listener); }

		// PhysicsWorldのイベント通知先を削除する。
		void RemovePhysicsEventListener(IPhysicsEventListener* listener) { eventDispatcher_.RemoveListener(listener); }

		// 位置補正ソルバーの有効状態を設定する。
		void SetPositionSolveEnabled(bool enabled) { positionSolveEnabled_ = enabled; }
		bool IsPositionSolveEnabled() const { return positionSolveEnabled_; }

		// 速度補正ソルバーの有効状態を設定する。
		void SetVelocitySolveEnabled(bool enabled) { velocitySolveEnabled_ = enabled; }
		bool IsVelocitySolveEnabled() const { return velocitySolveEnabled_; }

		// 摩擦ソルバーの有効状態を設定する。
		void SetFrictionSolveEnabled(bool enabled) { frictionSolveEnabled_ = enabled; }
		bool IsFrictionSolveEnabled() const { return frictionSolveEnabled_; }

		// 固定更新の有効状態を設定する。
		void SetUseFixedStep(bool useFixedStep);
		bool IsUseFixedStep() const { return useFixedStep_; }

		// 固定更新の1ステップ時間を設定する。
		void SetFixedTimeStep(float fixedTimeStep);
		float GetFixedTimeStep() const { return fixedTimeStep_; }

		// 1フレームで受け付ける最大deltaTimeを設定する。
		void SetMaxDeltaTime(float maxDeltaTime);
		float GetMaxDeltaTime() const { return maxDeltaTime_; }

		// 1フレームで実行する最大サブステップ数を設定する。
		void SetMaxSubSteps(int maxSubSteps);
		int GetMaxSubSteps() const { return maxSubSteps_; }

		float GetAccumulator() const { return accumulator_; }
		int GetLastSubStepCount() const { return lastSubStepCount_; }

		// CollisionLayer同士の応答設定を取得する。
		CollisionResponseMatrix& GetResponseMatrix() { return responseMatrix_; }
		const CollisionResponseMatrix& GetResponseMatrix() const { return responseMatrix_; }

		// PhysicsWorldが各Rigidbodyへ反映する重力加速度を設定する。
		void SetGravity(const Vector3& gravity) { gravity_ = gravity; }
		Vector3 GetGravity() const { return gravity_; }

	private: /// ---------- メンバ関数 ---------- ///

		// Rigidbodyの接地などのフレーム状態をリセットする。
		void ClearRigidbodyFrameState();

		// RigidbodyのSleep状態を更新する。
		void UpdateRigidbodySleepState(float deltaTime);

		// Contact normalから床接触状態を更新する。
		void UpdateGroundedState(const Contact& contact) const;

		// Colliderペアが接触しているかを既存Primitive判定で調べる。
		bool TestCollisionPair(Collider* colliderA, Collider* colliderB) const;

		// ColliderペアからContactを構築する。
		Contact BuildContact(Collider* colliderA, Collider* colliderB, CollisionResponseType response) const;

		// Rigidbodyの速度をClampして極端な値を抑制する
		void ClampRigidbodyVelocities();

	private: /// ---------- メンバ変数 ---------- ///

		// 物理ワールドが参照するCollider一覧。所有権は持たない。
		std::vector<Collider*> colliders_{};

		// 物理ワールドが参照するRigidbody一覧。所有権は持たない。
		std::vector<Rigidbody*> rigidbodies_{};

		// 直近ステップで検出されたContact一覧。
		std::vector<Contact> contacts_{};

		// CollisionLayer同士のIgnore/Trigger/Block設定。
		CollisionResponseMatrix responseMatrix_{};

		// Contact差分からCollision/Triggerイベントを生成する。
		PhysicsEventDispatcher eventDispatcher_{};

		// Contact解決で位置補正を行うか。DebugSceneからON/OFFを切り替える。
		bool positionSolveEnabled_ = true;

		// Contact解決で速度補正を行うか。DebugSceneからON/OFFを切り替える。
		bool velocitySolveEnabled_ = true;

		// Contact解決で摩擦補正を行うか。DebugSceneからON/OFFを切り替える。
		bool frictionSolveEnabled_ = true;

		// 登録済みRigidbodyへ反映する重力加速度。
		Vector3 gravity_{ 0.0f, -9.8f, 0.0f };

		// 固定更新とサブステップ制御用の状態。
		bool useFixedStep_ = true;
		float fixedTimeStep_ = 1.0f / 60.0f;
		float maxDeltaTime_ = 0.1f;
		float accumulator_ = 0.0f;
		int maxSubSteps_ = 4;
		int lastSubStepCount_ = 0;
	};

} // namespace Ken4lowEngine
