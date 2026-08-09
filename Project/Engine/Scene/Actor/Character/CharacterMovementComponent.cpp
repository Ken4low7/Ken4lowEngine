#define NOMINMAX
#include "CharacterMovementComponent.h"

#include "Actor.h"
#include "CharacterColliderComponent.h"
#include "SceneComponent.h"
#include <RigidbodyComponent.h>
#include <Stage.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDirectionEpsilon = 0.0001f;
		constexpr float kGravityAcceleration = 9.81f;

		/// Yaw差分を-πから+πへ正規化し、常に短い向きへ回転させる。
		float WrapAngle(float angle)
		{
			constexpr float pi = std::numbers::pi_v<float>;
			constexpr float twoPi = std::numbers::pi_v<float> * 2.0f;
			angle = std::fmod(angle + pi, twoPi);
			if (angle < 0.0f) angle += twoPi;
			return angle - pi;
		}

		/// 被弾方向をXZ平面で安全に正規化する。
		Vector3 NormalizeKnockbackDirection(const Vector3& direction)
		{
			const float length = Vector3::LengthXZ(direction);
			if (length <= kDirectionEpsilon) return { 0.0f, 0.0f, 1.0f };
			return { direction.x / length, 0.0f, direction.z / length };
		}

		/// 進行線分とAgent半径で広げた障害物をXZ平面上で判定し、最初に接触する割合を返す。
		bool SegmentIntersectsExpandedAabbXZ(
			const Vector3& from,
			const Vector3& to,
			const AABB& obstacle,
			float padding,
			float& outEnterT)
		{
			const Vector3 delta = to - from;
			float enterT = 0.0f;
			float exitT = 1.0f;
			const float safePadding = std::max(0.0f, padding);

			const auto updateAxis = [](float origin, float direction, float minValue, float maxValue, float& inOutEnterT, float& inOutExitT)
				{
					if (std::abs(direction) <= 0.000001f) return origin >= minValue && origin <= maxValue;
					const float inverse = 1.0f / direction;
					float axisEnter = (minValue - origin) * inverse;
					float axisExit = (maxValue - origin) * inverse;
					if (axisEnter > axisExit) std::swap(axisEnter, axisExit);
					inOutEnterT = std::max(inOutEnterT, axisEnter);
					inOutExitT = std::min(inOutExitT, axisExit);
					return inOutEnterT <= inOutExitT;
				};

			if (!updateAxis(from.x, delta.x, obstacle.min.x - safePadding, obstacle.max.x + safePadding, enterT, exitT)) return false;
			if (!updateAxis(from.z, delta.z, obstacle.min.z - safePadding, obstacle.max.z + safePadding, enterT, exitT)) return false;
			if (exitT < 0.0f || enterT > 1.0f) return false;
			outEnterT = std::clamp(enterT, 0.0f, 1.0f);
			return true;
		}
	}

	void CharacterMovementComponent::Update(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
		if (ApplyMotorTargetToRigidbody(deltaTime)) return;
		if (!movementEnabled_) return;
		ApplyMovement(deltaTime);
	}

	void CharacterMovementComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("キャラクター移動");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
#endif
	}

	void CharacterMovementComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson);
		ComponentPropertyUtility::ToJson(const_cast<CharacterMovementComponent*>(this)->CreateProperties(), outJson);
	}

	void CharacterMovementComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	void CharacterMovementComponent::ApplyDamageKnockback(const Vector3& direction, float horizontalPower, float verticalPower)
	{
		const Vector3 normalized = NormalizeKnockbackDirection(direction);
		const float safeHorizontalPower = std::isfinite(horizontalPower) ? std::max(0.0f, horizontalPower) : 0.0f;
		const float safeVerticalPower = std::isfinite(verticalPower) ? std::max(0.0f, verticalPower) : 0.0f;
		velocity_.x = normalized.x * safeHorizontalPower;
		velocity_.z = normalized.z * safeHorizontalPower;

		Actor* owner = GetOwner();
		RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
		Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
		if (!rigidbody) return;

		Vector3 physicalVelocity = rigidbody->GetVelocity();
		physicalVelocity.x = velocity_.x;
		physicalVelocity.z = velocity_.z;
		physicalVelocity.y = std::max(physicalVelocity.y, safeVerticalPower);
		rigidbodyComponent->SetVelocity(physicalVelocity); // 被弾フレームのPhysics Stepへ水平押し出しと浮き上がりを即時反映する。
	}

	void CharacterMovementComponent::SetVelocity(const Vector3& velocity)
	{
		velocity_.x = std::isfinite(velocity.x) ? velocity.x : 0.0f;
		velocity_.y = std::isfinite(velocity.y) ? velocity.y : 0.0f;
		velocity_.z = std::isfinite(velocity.z) ? velocity.z : 0.0f;
	}

	bool CharacterMovementComponent::ApplyMotorTargetToRigidbody(float deltaTime)
	{
		if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return false;
		Actor* owner = GetOwner();
		RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
		Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
		if (!rigidbody) return false;

		Vector3 physicalVelocity = rigidbody->GetVelocity();
		const Vector3 targetVelocity = movementEnabled_ ? velocity_ : Vector3{};
		TryStartAutomaticObstacleTraversal(physicalVelocity, targetVelocity, deltaTime);
		const float deltaX = targetVelocity.x - physicalVelocity.x;
		const float deltaZ = targetVelocity.z - physicalVelocity.z;
		const float deltaSpeed = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);

		if (deltaSpeed > 0.000001f)
		{
			const float targetHorizontalSpeedSq = targetVelocity.x * targetVelocity.x + targetVelocity.z * targetVelocity.z;
			const float forceLimit = targetHorizontalSpeedSq > 0.000001f ? maxDriveForce_ : maxBrakingForce_;
			const float maxDeltaSpeed = forceLimit * std::max(rigidbody->GetInvMass(), 0.0f) * deltaTime;
			const float appliedDeltaSpeed = std::min(deltaSpeed, maxDeltaSpeed);
			const float ratio = deltaSpeed > 0.0f ? appliedDeltaSpeed / deltaSpeed : 0.0f;
			physicalVelocity.x += deltaX * ratio;
			physicalVelocity.z += deltaZ * ratio;
		}

		rigidbodyComponent->SetVelocity(physicalVelocity); // Y速度は重力・ジャンプ・衝突Impulseの結果を保持する。
		return true;
	}

	void CharacterMovementComponent::SetMaxDriveForce(float force)
	{
		maxDriveForce_ = std::isfinite(force) ? std::max(0.0f, force) : 0.0f;
	}

	void CharacterMovementComponent::SetMaxBrakingForce(float force)
	{
		maxBrakingForce_ = std::isfinite(force) ? std::max(0.0f, force) : 0.0f;
	}

	void CharacterMovementComponent::ConfigureAutomaticObstacleTraversal(
		bool enabled,
		float maxClimbHeight,
		float lookAheadDistance,
		float minimumJumpSpeed,
		float cooldown)
	{
		automaticObstacleTraversalEnabled_ = enabled;
		automaticObstacleMaxClimbHeight_ = std::isfinite(maxClimbHeight) ? std::max(0.0f, maxClimbHeight) : 0.0f;
		automaticObstacleLookAheadDistance_ = std::isfinite(lookAheadDistance) ? std::max(0.1f, lookAheadDistance) : 0.1f;
		automaticObstacleMinimumJumpSpeed_ = std::isfinite(minimumJumpSpeed) ? std::max(0.0f, minimumJumpSpeed) : 0.0f;
		automaticObstacleCooldown_ = std::isfinite(cooldown) ? std::max(0.0f, cooldown) : 0.0f;
		if (!enabled) automaticObstacleCooldownTimer_ = 0.0f;
	}

	bool CharacterMovementComponent::TryStartAutomaticObstacleTraversal(
		Vector3& physicalVelocity,
		const Vector3& targetVelocity,
		float deltaTime)
	{
		automaticObstacleCooldownTimer_ = std::max(0.0f, automaticObstacleCooldownTimer_ - std::max(0.0f, deltaTime));
		if (!automaticObstacleTraversalEnabled_ || !movementEnabled_ || automaticObstacleCooldownTimer_ > 0.0f) return false;
		if (automaticObstacleMaxClimbHeight_ <= 0.0f) return false;

		const float horizontalSpeed = Vector3::LengthXZ(targetVelocity);
		if (horizontalSpeed <= kDirectionEpsilon) return false;
		const Vector3 direction{ targetVelocity.x / horizontalSpeed, 0.0f, targetVelocity.z / horizontalSpeed };

		Actor* owner = GetOwner();
		RigidbodyComponent* rigidbodyComponent = owner ? owner->GetComponent<RigidbodyComponent>() : nullptr;
		Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
		const CharacterColliderComponent* collider = owner ? owner->GetComponent<CharacterColliderComponent>() : nullptr;
		const SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		Stage* stage = Stage::GetActiveRuntimeStage();
		if (!owner || !rigidbody || !rigidbody->IsGrounded() || (!collider && !root) || !stage) return false; // 壁へ触れた空中状態では再ジャンプせず、接地中の最初の一回だけ乗越を開始する。
		if (std::abs(physicalVelocity.y) > 1.2f) return false;

		const Vector3 halfSize = collider ? collider->GetHalfSize() : Vector3{ 0.7f, 2.0f, 0.7f };
		const Vector3 current = collider ? collider->GetWorldPosition() : root->GetWorldPosition();
		const float agentRadius = std::max(0.2f, std::max(halfSize.x, halfSize.z));
		const float footY = current.y - std::max(0.2f, halfSize.y);
		const Vector3 lookAheadEnd = current + direction * (automaticObstacleLookAheadDistance_ + agentRadius);

		float nearestEnterT = std::numeric_limits<float>::max();
		float selectedClimbHeight = 0.0f;
		const auto considerSurface = [&](const AABB& obstacle)
			{
				const float climbHeight = obstacle.max.y - footY;
				if (climbHeight <= 0.12f || climbHeight > automaticObstacleMaxClimbHeight_) return;
				if (obstacle.min.y > footY + 0.45f) return;

				float enterT = 0.0f;
				if (!SegmentIntersectsExpandedAabbXZ(current, lookAheadEnd, obstacle, agentRadius + 0.08f, enterT)) return;
				if (enterT >= nearestEnterT) return;
				nearestEnterT = enterT;
				selectedClimbHeight = climbHeight;
			};

		const auto& wallObstacles = stage->GetWallObstacleAABBs();
		const auto& walkableFlags = stage->GetWallObstacleWalkable();
		for (size_t index = 0; index < wallObstacles.size(); ++index)
		{
			if (index >= walkableFlags.size() || walkableFlags[index] == 0u) continue;
			considerSurface(wallObstacles[index]); // 壁・木・柱は除外し、上面へ立てる低い遮蔽物だけを自動乗越の対象にする。
		}
		for (const AABB& floor : stage->GetFloorAABBs()) considerSurface(floor);

		if (selectedClimbHeight <= 0.0f) return false;
		const float requiredJumpSpeed = std::sqrt(2.0f * kGravityAcceleration * (selectedClimbHeight + 0.45f));
		physicalVelocity.y = std::max(physicalVelocity.y, std::clamp(std::max(automaticObstacleMinimumJumpSpeed_, requiredJumpSpeed), 0.0f, 12.0f));
		automaticObstacleCooldownTimer_ = automaticObstacleCooldown_;
		return true; // 水平Motorを維持したまま上向き速度だけを加え、上面への着地と反対側への降下はPhysicsへ任せる。
	}

	Vector3 CharacterMovementComponent::CalculateDisplacement(float deltaTime) const
	{
		if (!movementEnabled_ || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return {};
		return velocity_ * deltaTime;
	}

	bool CharacterMovementComponent::FaceDirectionXZ(const Vector3& direction, float rotateSpeed, float deltaTime)
	{
		Actor* owner = GetOwner();
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		const float length = Vector3::LengthXZ(direction);
		if (!root || length < kDirectionEpsilon || !std::isfinite(rotateSpeed) || rotateSpeed < 0.0f || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return false;

		const Vector3 normalized{ direction.x / length, 0.0f, direction.z / length };
		const float targetYaw = std::atan2(-normalized.x, normalized.z);
		Vector3 rotation = root->GetLocalRotation();
		const float maxStep = rotateSpeed * deltaTime;
		const float deltaYaw = std::clamp(WrapAngle(targetYaw - rotation.y), -maxStep, maxStep);
		rotation.y = WrapAngle(rotation.y + deltaYaw);
		root->SetLocalRotation(rotation);
		root->RefreshWorldTransform();
		return true;
	}

	void CharacterMovementComponent::ApplyMovement(float deltaTime)
	{
		Actor* owner = GetOwner();
		SceneComponent* root = owner ? owner->GetRootComponent() : nullptr;
		if (!root) return;

		const Vector3& position = root->GetLocalPosition();
		const Vector3 displacement = CalculateDisplacement(deltaTime);
		root->SetLocalPosition({
			position.x + displacement.x,
			position.y + displacement.y,
			position.z + displacement.z
			});
		root->RefreshWorldTransform();
	}

	std::vector<ComponentProperty> CharacterMovementComponent::CreateProperties()
	{
		return {
			{ "Velocity", "目標速度", ComponentPropertyType::Vector3, [this]() -> ComponentPropertyValue { return velocity_; }, [this](const ComponentPropertyValue& value) { if (const Vector3* typedValue = std::get_if<Vector3>(&value)) SetVelocity(*typedValue); }, 0.0f, 0.0f, 0.05f, false, {}, ComponentPropertyDisplay::Default },
			{ "MaxDriveForce", "最大駆動力", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxDriveForce_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetMaxDriveForce(*typedValue); }, 0.0f, 5000.0f, 1.0f, true },
			{ "MaxBrakingForce", "最大制動力", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxBrakingForce_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) SetMaxBrakingForce(*typedValue); }, 0.0f, 5000.0f, 1.0f, true },
			{ "AutomaticObstacleTraversalEnabled", "低障害物の自動乗越", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return automaticObstacleTraversalEnabled_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) automaticObstacleTraversalEnabled_ = *typedValue; }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::Default },
			{ "AutomaticObstacleMaxClimbHeight", "自動乗越の最大高さ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return automaticObstacleMaxClimbHeight_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) automaticObstacleMaxClimbHeight_ = std::max(0.0f, *typedValue); }, 0.0f, 8.0f, 0.05f, true },
			{ "AutomaticObstacleLookAheadDistance", "自動乗越の前方距離", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return automaticObstacleLookAheadDistance_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) automaticObstacleLookAheadDistance_ = std::max(0.1f, *typedValue); }, 0.1f, 8.0f, 0.05f, true },
			{ "AutomaticObstacleMinimumJumpSpeed", "自動乗越の最低上昇速度", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return automaticObstacleMinimumJumpSpeed_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) automaticObstacleMinimumJumpSpeed_ = std::max(0.0f, *typedValue); }, 0.0f, 20.0f, 0.1f, true },
			{ "AutomaticObstacleCooldown", "自動乗越の再実行間隔", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return automaticObstacleCooldown_; }, [this](const ComponentPropertyValue& value) { if (const float* typedValue = std::get_if<float>(&value)) automaticObstacleCooldown_ = std::max(0.0f, *typedValue); }, 0.0f, 3.0f, 0.05f, true },
			{ "MovementEnabled", "移動有効", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return movementEnabled_; }, [this](const ComponentPropertyValue& value) { if (const bool* typedValue = std::get_if<bool>(&value)) SetMovementEnabled(*typedValue); }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::Default }
		};
	}
} // namespace Ken4lowEngine
