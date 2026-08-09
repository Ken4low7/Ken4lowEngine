#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "EnemyAIComponent.h"

#include "EnemyAttackComponent.h"

#include <Scene/Actor/Character/CharacterActor.h>
#include <Scene/Actor/Character/CharacterColliderComponent.h>
#include <Scene/Actor/Character/CharacterMovementComponent.h>
#include <SceneComponent.h>
#include <Stage.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDirectionEpsilon = 0.0001f;
		constexpr float kGoldenAngle = 2.39996323f;

		Vector3 NormalizeDirectionXZ(const Vector3& direction)
		{
			const float length = Vector3::LengthXZ(direction);
			if (length < kDirectionEpsilon) return {};
			return { direction.x / length, 0.0f, direction.z / length };
		}

		std::uint32_t BuildWanderSeed(const Vector3& position)
		{
			const auto quantize = [](float value)
				{
					return static_cast<std::uint32_t>(std::abs(std::floor(value * 100.0f)));
				};
			std::uint32_t seed = quantize(position.x) * 73856093u;
			seed ^= quantize(position.z) * 19349663u;
			return seed == 0u ? 1u : seed;
		}
	}

	void EnemyAIComponent::Initialize()
	{
		EnemyAStarNavigator::Settings settings{};
		settings.cellSize = 1.5f;
		settings.agentRadius = 0.9f;
		settings.agentHalfHeight = 2.0f;
		settings.floorHeightTolerance = 3.1f; // 2.8m以下の遮蔽物上から地面へ降りる経路も同じNavigation層として探索する。
		settings.searchRangeCells = 32;
		settings.maxExpandedNodes = 6000;
		settings.waypointReachDistance = 0.85f;
		settings.repathIntervalSec = 0.25f;
		settings.disableCornerCutting = true;

		if (const auto* owner = dynamic_cast<const CharacterActor*>(GetOwner()))
		{
			if (const CharacterColliderComponent* collider = owner->GetColliderComponent())
			{
				const Vector3 halfSize = collider->GetHalfSize();
				settings.agentRadius = std::max(0.2f, std::max(halfSize.x, halfSize.z));
				settings.agentHalfHeight = std::max(0.2f, halfSize.y);
			}
		}
		navigator_.SetSettings(settings); // Collider全高で障害物を判定し、低いBlockも中心点の下として無視しない。

		if (Stage* stage = Stage::GetActiveRuntimeStage())
		{
			if (!navigationObstacles_) navigationObstacles_ = &stage->GetNavigationObstacleAABBs();
			if (!walkableAreas_) walkableAreas_ = &stage->GetFloorAABBs();
		}
		navigator_.SetWorldAABBs(navigationObstacles_);
		navigator_.SetWalkableAABBs(walkableAreas_);
		ResetBehavior();
	}

	void EnemyAIComponent::Update(float deltaTime)
	{
		auto* owner = dynamic_cast<CharacterActor*>(GetOwner());
		CharacterMovementComponent* movement = owner ? owner->GetMovementComponent() : nullptr;
		EnemyAttackComponent* attack = owner ? owner->GetCharacterComponent<EnemyAttackComponent>() : nullptr;
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		if (!owner || !movement || !root || !behaviorEnabled_ || owner->IsDead())
		{
			if (movement) movement->Stop();
			stateName_ = owner && owner->IsDead() ? "Dead" : "Disabled";
			return;
		}

		if (!walkableAreas_)
		{
			if (Stage* stage = Stage::GetActiveRuntimeStage()) SetWalkableAreas(&stage->GetFloorAABBs());
		}
		navigator_.TickTemporaryBlocks(std::max(0.0f, deltaTime));

		if (!spawnOriginCaptured_)
		{
			spawnOrigin_ = root->GetWorldPosition();
			wanderTarget_ = spawnOrigin_;
			wanderSequence_ = BuildWanderSeed(spawnOrigin_);
			spawnOriginCaptured_ = true; // Spawn座標から個体別Sequenceを作り、全個体が同じ巡回地点へ集中しないようにする。
		}

		if (attack && attack->IsAttacking())
		{
			movement->Stop();
			stateName_ = "Attacking";
			return;
		}

		const Vector3 current = root->GetWorldPosition();
		const bool hasLivingTarget = targetActor_ && !targetActor_->IsDead();
		if (hasLivingTarget)
		{
			const Vector3 target = targetActor_->GetTargetPosition();
			const Vector3 toTarget = target - current;
			distanceToTarget_ = Vector3::LengthXZ(toTarget);
			if (distanceToTarget_ <= detectRange_)
			{
				if (attack && attack->ShouldHoldForAttack())
				{
					movement->Stop();
					movement->FaceDirectionXZ(toTarget, rotateSpeed_, deltaTime);
					pathFound_ = false;
					stateName_ = "AttackRange";
					return;
				}

				FollowNavigationGoal(
					*owner,
					*movement,
					current,
					target,
					GetChaseSpeed(),
					rotateSpeed_,
					deltaTime,
					"ChasePath",
					"ChasePathFailed");
				return; // A*失敗時もTargetへ直進せず、その場で再探索を待つ。
			}
		}
		else
		{
			distanceToTarget_ = 0.0f;
		}

		if (!wanderEnabled_)
		{
			movement->Stop();
			pathFound_ = false;
			stateName_ = hasLivingTarget ? "TargetOutOfRange" : "Idle";
			return;
		}

		wanderTimer_ = std::max(0.0f, wanderTimer_ - std::max(0.0f, deltaTime));
		const bool reachedWanderTarget = wanderTargetValid_ && Vector3::LengthXZ(wanderTarget_ - current) <= 1.0f;
		if (!wanderTargetValid_ || reachedWanderTarget || wanderTimer_ <= 0.0f)
		{
			wanderTargetValid_ = SelectNextWanderTarget(current, GetNavigationSampleY(*owner));
			wanderTimer_ = wanderTargetValid_ ? wanderInterval_ : wanderRetryDelay_;
			navigator_.Reset();
		}

		if (!wanderTargetValid_)
		{
			movement->Stop();
			pathFound_ = false;
			stateName_ = "WanderGoalFailed";
			return;
		}

		if (!FollowNavigationGoal(
			*owner,
			*movement,
			current,
			wanderTarget_,
			moveSpeed_ * wanderSpeedScale_,
			rotateSpeed_ * 0.65f,
			deltaTime,
			"WanderPath",
			"WanderPathFailed"))
		{
			wanderTargetValid_ = false;
			wanderTimer_ = wanderRetryDelay_;
		}
	}

	float EnemyAIComponent::GetNavigationSampleY(const CharacterActor& owner) const
	{
		if (const CharacterColliderComponent* collider = owner.GetColliderComponent()) return collider->GetWorldPosition().y;
		if (const SceneComponent* root = owner.GetRootComponent()) return root->GetWorldPosition().y;
		return 0.0f;
	}

	bool EnemyAIComponent::FollowNavigationGoal(
		CharacterActor& owner,
		CharacterMovementComponent& movement,
		const Vector3& current,
		const Vector3& goal,
		float speed,
		float rotateSpeed,
		float deltaTime,
		const char* successState,
		const char* failureState)
	{
		Vector3 waypoint = current;
		const float sampleY = GetNavigationSampleY(owner);
		pathFound_ = navigator_.GetNextWaypoint(current, goal, sampleY, deltaTime, waypoint);
		if (!pathFound_)
		{
			movement.Stop();
			stateName_ = failureState ? failureState : "PathFailed";
			return false;
		}

		int blockedObstacleIndex = -1;
		if (navigator_.IsSegmentBlockedByObstacle(current, waypoint, sampleY, &blockedObstacleIndex))
		{
			navigator_.Reset();
			movement.Stop();
			pathFound_ = false;
			stateName_ = "WaypointBlocked";
			return false;
		}

		const Vector3 direction = NormalizeDirectionXZ(waypoint - current);
		if (Vector3::LengthXZ(direction) < kDirectionEpsilon)
		{
			movement.Stop();
			stateName_ = successState ? successState : "PathReached";
			return true;
		}

		movement.FaceDirectionXZ(direction, rotateSpeed, deltaTime);
		movement.SetVelocity(direction * std::max(0.0f, speed));
		stateName_ = successState ? successState : "PathMove";
		return true;
	}

	bool EnemyAIComponent::SelectNextWanderTarget(const Vector3& current, float sampleY)
	{
		const float minimumDistance = std::max(2.0f, wanderRadius_ * 0.5f);
		if (navigator_.TrySelectPatrolGoal(current, sampleY, wanderSequence_++, minimumDistance, wanderTarget_)) return true;

		wanderAngle_ = std::fmod(wanderAngle_ + kGoldenAngle, std::numbers::pi_v<float> * 2.0f);
		wanderTarget_ = current + Vector3{
			std::cos(wanderAngle_) * std::max(4.0f, wanderRadius_),
			0.0f,
			std::sin(wanderAngle_) * std::max(4.0f, wanderRadius_)
		};
		return walkableAreas_ == nullptr || walkableAreas_->empty(); // Floor未接続のDebug環境だけ旧方向徘徊へフォールバックする。
	}

	void EnemyAIComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("通常敵AI");
		ImGui::Text("状態: %s", stateName_.c_str());
		ImGui::Text("基本速度: %.2f / 追跡速度: %.2f", moveSpeed_, GetChaseSpeed());
		ImGui::Text("追跡倍率: %.2f / 索敵距離: %.2f", chaseSpeedMultiplier_, detectRange_);
		ImGui::Text("Target水平距離: %.2f", distanceToTarget_);
		ImGui::Text("巡回目標: (%.1f, %.1f) / 有効:%s", wanderTarget_.x, wanderTarget_.z, wanderTargetValid_ ? "Yes" : "No");
		ImGui::Text("Stage Floor: %zu / Obstacle: %zu", navigator_.GetWalkableAreaCount(), navigator_.GetObstacleCount());
		ImGui::Text("A*経路: %s / %zu nodes", pathFound_ ? "有効" : "未生成", navigator_.GetCurrentPath().size());
#endif
	}

	void EnemyAIComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		outJson["MoveSpeed"] = moveSpeed_;
		outJson["ChaseSpeedMultiplier"] = chaseSpeedMultiplier_;
		outJson["RotateSpeed"] = rotateSpeed_;
		outJson["StopDistance"] = stopDistance_;
		outJson["AttackStartRange"] = attackStartRange_;
		outJson["DetectRange"] = detectRange_;
		outJson["WanderEnabled"] = wanderEnabled_;
		outJson["WanderRadius"] = wanderRadius_;
		outJson["WanderInterval"] = wanderInterval_;
		outJson["WanderSpeedScale"] = wanderSpeedScale_;
		outJson["WanderRetryDelay"] = wanderRetryDelay_;
	}

	void EnemyAIComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		moveSpeed_ = std::max(0.0f, inJson.value("MoveSpeed", moveSpeed_));
		chaseSpeedMultiplier_ = std::clamp(inJson.value("ChaseSpeedMultiplier", chaseSpeedMultiplier_), 0.1f, 4.0f);
		rotateSpeed_ = std::max(0.0f, inJson.value("RotateSpeed", rotateSpeed_));
		stopDistance_ = std::max(0.0f, inJson.value("StopDistance", stopDistance_));
		attackStartRange_ = std::max(stopDistance_, inJson.value("AttackStartRange", attackStartRange_));
		detectRange_ = std::max(attackStartRange_, inJson.value("DetectRange", detectRange_));
		wanderEnabled_ = inJson.value("WanderEnabled", wanderEnabled_);
		wanderRadius_ = std::max(0.0f, inJson.value("WanderRadius", wanderRadius_));
		wanderInterval_ = std::max(0.5f, inJson.value("WanderInterval", wanderInterval_));
		wanderSpeedScale_ = std::clamp(inJson.value("WanderSpeedScale", wanderSpeedScale_), 0.0f, 1.0f);
		wanderRetryDelay_ = std::clamp(inJson.value("WanderRetryDelay", wanderRetryDelay_), 0.1f, 2.0f);
	}

	void EnemyAIComponent::SetNavigationObstacles(const std::vector<AABB>* obstacles)
	{
		navigationObstacles_ = obstacles;
		navigator_.SetWorldAABBs(navigationObstacles_);
		if (Stage* stage = Stage::GetActiveRuntimeStage()) SetWalkableAreas(&stage->GetFloorAABBs());
	}

	void EnemyAIComponent::SetWalkableAreas(const std::vector<AABB>* walkableAreas)
	{
		if (walkableAreas_ == walkableAreas) return;
		walkableAreas_ = walkableAreas;
		navigator_.SetWalkableAABBs(walkableAreas_);
		wanderTargetValid_ = false; // Stage切替後は旧ステージの巡回目標を破棄する。
	}

	void EnemyAIComponent::ApplyMoveSpeedMultiplier(float multiplier)
	{
		moveSpeed_ *= std::max(0.1f, multiplier);
	}

	void EnemyAIComponent::StopBehavior()
	{
		behaviorEnabled_ = false;
		if (auto* owner = dynamic_cast<CharacterActor*>(GetOwner()))
		{
			if (CharacterMovementComponent* movement = owner->GetMovementComponent()) movement->Stop();
		}
		pathFound_ = false;
		navigator_.Reset();
		stateName_ = "Stopped";
	}

	void EnemyAIComponent::ResetBehavior()
	{
		behaviorEnabled_ = true;
		pathFound_ = false;
		spawnOriginCaptured_ = false;
		wanderTargetValid_ = false;
		wanderTimer_ = 0.0f;
		wanderAngle_ = 0.0f;
		wanderSequence_ = 1u;
		distanceToTarget_ = 0.0f;
		stateName_ = "Idle";
		navigator_.Reset();
		navigator_.ClearTemporaryBlockedAreas();
	}
} // namespace Ken4lowEngine
