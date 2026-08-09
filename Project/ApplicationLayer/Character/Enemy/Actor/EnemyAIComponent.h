#pragma once

#include "ApplicationLayer/Character/Enemy/Navigation/EnemyAStarNavigator.h"

#include <ActorComponent.h>
#include <AABB.h>
#include <Vector3.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class CharacterActor;
	class CharacterMovementComponent;

	/// 通常敵の索敵・Stage巡回・追跡判断とA*経路選択を、Actor本体や描画処理から分離して管理するComponent。
	class EnemyAIComponent final : public ActorComponent
	{
	public:
		/// 旧MeleeEnemyと同じ移動・停止値を基準に、Collider寸法を使うA*を初期化する。
		void Initialize() override;

		/// Targetの索敵状態に応じてStage巡回またはA*追跡を選び、移動要求をCharacterMovementComponentへ渡す。
		void Update(float deltaTime) override;

		/// AI調整値、Stage接続数、現在の経路状態をDebug表示する。
		void DrawImGui() override;

		std::string GetClassTypeName() const override { return "EnemyAIComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }

		/// A*で回避対象にするStage障害物を設定する。所有権は移さない。
		void SetNavigationObstacles(const std::vector<AABB>* obstacles);

		/// 巡回目標とA*のStage外判定に使うFloor AABBを設定する。所有権は移さない。
		void SetWalkableAreas(const std::vector<AABB>* walkableAreas);

		void ApplyMoveSpeedMultiplier(float multiplier);
		float GetMoveSpeed() const { return moveSpeed_; }
		float GetChaseSpeed() const { return moveSpeed_ * chaseSpeedMultiplier_; }
		float GetRotateSpeed() const { return rotateSpeed_; }
		float GetAttackStartRange() const { return attackStartRange_; }
		bool HasPath() const { return pathFound_; }
		size_t GetPathNodeCount() const { return navigator_.GetCurrentPath().size(); }
		const std::string& GetStateName() const { return stateName_; }
		float GetDistanceToTarget() const { return distanceToTarget_; }
		const Vector3& GetWanderTarget() const { return wanderTarget_; }

		void StopBehavior();
		void ResetBehavior();

	private:
		float GetNavigationSampleY(const CharacterActor& owner) const;
		bool FollowNavigationGoal(
			CharacterActor& owner,
			CharacterMovementComponent& movement,
			const Vector3& current,
			const Vector3& goal,
			float speed,
			float rotateSpeed,
			float deltaTime,
			const char* successState,
			const char* failureState);
		bool SelectNextWanderTarget(const Vector3& current, float sampleY);

	private:
		CharacterActor* targetActor_ = nullptr;
		const std::vector<AABB>* navigationObstacles_ = nullptr;
		const std::vector<AABB>* walkableAreas_ = nullptr;
		EnemyAStarNavigator navigator_{};
		Vector3 spawnOrigin_{};
		Vector3 wanderTarget_{};
		float moveSpeed_ = 4.0f;
		float chaseSpeedMultiplier_ = 1.4f;
		float rotateSpeed_ = 9.0f;
		float stopDistance_ = 1.8f;
		float attackStartRange_ = 2.4f;
		float detectRange_ = 18.0f;
		float wanderRadius_ = 18.0f;
		float wanderInterval_ = 7.0f;
		float wanderSpeedScale_ = 0.65f;
		float wanderRetryDelay_ = 0.35f;
		float wanderTimer_ = 0.0f;
		float wanderAngle_ = 0.0f;
		float distanceToTarget_ = 0.0f;
		std::uint32_t wanderSequence_ = 1u;
		bool pathFound_ = false;
		bool behaviorEnabled_ = true;
		bool wanderEnabled_ = true;
		bool spawnOriginCaptured_ = false;
		bool wanderTargetValid_ = false;
		std::string stateName_ = "Idle";
	};
} // namespace Ken4lowEngine