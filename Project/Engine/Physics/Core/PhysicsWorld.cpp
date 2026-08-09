#include "PhysicsWorld.h"

#include "Collider.h"
#include "CollisionUtility.h"
#include "FrictionSolver.h"
#include "PositionSolver.h"
#include "VelocitySolver.h"
#include "Rigidbody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Ken4lowEngine
{
	namespace
	{
		template<class T>
		bool ContainsPointer(const std::vector<T*>& values, T* value)
		{
			return std::find(values.begin(), values.end(), value) != values.end();
		}

		template<class T>
		void ErasePointer(std::vector<T*>& values, T* value)
		{
			values.erase(std::remove(values.begin(), values.end(), value), values.end());
		}

		bool IsBoxShape(ECollisionShapeType shapeType)
		{
			return shapeType == ECollisionShapeType::AABB || shapeType == ECollisionShapeType::OBB;
		}

		OBB BuildContactBox(const Collider& collider)
		{
			if (collider.GetShapeType() == ECollisionShapeType::OBB) return collider.GetOBB();

			const AABB aabb = collider.GetAABB();
			OBB box{};
			box.center = (aabb.min + aabb.max) * 0.5f;
			box.size = (aabb.max - aabb.min) * 0.5f;
			box.orientations[0] = { 1.0f, 0.0f, 0.0f };
			box.orientations[1] = { 0.0f, 1.0f, 0.0f };
			box.orientations[2] = { 0.0f, 0.0f, 1.0f };
			return box;
		}

		float GetBoxExtent(const OBB& box, size_t axisIndex)
		{
			switch (axisIndex)
			{
			case 0: return box.size.x;
			case 1: return box.size.y;
			default: return box.size.z;
			}
		}

		float ProjectBoxRadius(const OBB& box, const Vector3& axis)
		{
			float radius = 0.0f;
			for (size_t i = 0; i < 3; ++i)
			{
				radius += std::fabs(Vector3::Dot(box.orientations[i], axis)) * GetBoxExtent(box, i);
			}
			return radius;
		}

		Vector3 GetBoxSupportPoint(const OBB& box, const Vector3& direction)
		{
			Vector3 point = box.center;
			for (size_t i = 0; i < 3; ++i)
			{
				const float sign = Vector3::Dot(box.orientations[i], direction) >= 0.0f ? 1.0f : -1.0f;
				point += box.orientations[i] * (GetBoxExtent(box, i) * sign);
			}
			return point;
		}

		bool BuildBoxContactData(const OBB& boxA, const OBB& boxB, Vector3& outNormal, float& outPenetration, Vector3& outPoint)
		{
			constexpr float kAxisEpsilonSq = 0.000001f;
			constexpr float kSeparationTolerance = 0.0001f;
			constexpr float kContactTieTolerance = 0.02f;
			const Vector3 centerDelta = boxB.center - boxA.center;
			const bool verticallySeparated = std::fabs(centerDelta.y) > 0.2f;
			float minimumPenetration = std::numeric_limits<float>::max();
			Vector3 minimumAxis{ 0.0f, 1.0f, 0.0f };

			const auto testAxis = [&](const Vector3& rawAxis)
				{
					const float lengthSq = Vector3::LengthSquared(rawAxis);
					if (lengthSq <= kAxisEpsilonSq) return true;
					const Vector3 axis = rawAxis * (1.0f / std::sqrt(lengthSq));
					const float radiusA = ProjectBoxRadius(boxA, axis);
					const float radiusB = ProjectBoxRadius(boxB, axis);
					const float signedDistance = Vector3::Dot(centerDelta, axis);
					const float penetration = radiusA + radiusB - std::fabs(signedDistance);
					if (penetration < -kSeparationTolerance) return false;

					const bool isFirstAxis = minimumPenetration == std::numeric_limits<float>::max();
					const bool hasSmallerPenetration = penetration < minimumPenetration - kContactTieTolerance;
					const bool isNearTie = !isFirstAxis && std::fabs(penetration - minimumPenetration) <= kContactTieTolerance;
					const bool prefersSupportNormal = verticallySeparated && isNearTie && std::fabs(axis.y) > std::fabs(minimumAxis.y) + 0.05f;
					if (isFirstAxis || hasSmallerPenetration || prefersSupportNormal)
					{
						minimumPenetration = std::max(0.0f, penetration);
						minimumAxis = signedDistance >= 0.0f ? axis : axis * -1.0f;
					}
					return true;
				};

			for (size_t i = 0; i < 3; ++i)
			{
				if (!testAxis(boxA.orientations[i]) || !testAxis(boxB.orientations[i])) return false;
			}
			for (size_t axisA = 0; axisA < 3; ++axisA)
			{
				for (size_t axisB = 0; axisB < 3; ++axisB)
				{
					if (!testAxis(Vector3::Cross(boxA.orientations[axisA], boxB.orientations[axisB]))) return false;
				}
			}

			if (minimumPenetration == std::numeric_limits<float>::max()) return false;
			outNormal = minimumAxis;
			outPenetration = minimumPenetration;
			const Vector3 pointA = GetBoxSupportPoint(boxA, outNormal);
			const Vector3 pointB = GetBoxSupportPoint(boxB, outNormal * -1.0f);
			outPoint = (pointA + pointB) * 0.5f; // 上面と側面の侵入量が同程度なら、縦に積まれたBoxは接地法線を優先して端で固着させない。
			return true;
		}

		void BuildFallbackAabbContact(Collider* colliderA, Collider* colliderB, Contact& contact)
		{
			const Vector3 centerA = colliderA->GetCenterPosition();
			const Vector3 centerB = colliderB->GetCenterPosition();
			const Vector3 delta = centerB - centerA;
			const AABB aabbA = colliderA->GetAABB();
			const AABB aabbB = colliderB->GetAABB();

			const float overlapX = std::min(aabbA.max.x, aabbB.max.x) - std::max(aabbA.min.x, aabbB.min.x);
			const float overlapY = std::min(aabbA.max.y, aabbB.max.y) - std::max(aabbA.min.y, aabbB.min.y);
			const float overlapZ = std::min(aabbA.max.z, aabbB.max.z) - std::max(aabbA.min.z, aabbB.min.z);
			const Vector3 overlapMin{
				std::max(aabbA.min.x, aabbB.min.x),
				std::max(aabbA.min.y, aabbB.min.y),
				std::max(aabbA.min.z, aabbB.min.z),
			};
			const Vector3 overlapMax{
				std::min(aabbA.max.x, aabbB.max.x),
				std::min(aabbA.max.y, aabbB.max.y),
				std::min(aabbA.max.z, aabbB.max.z),
			};

			contact.point = (overlapMin + overlapMax) * 0.5f;
			contact.penetration = std::max(0.0f, overlapX);
			contact.normal = { delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f };
			if (overlapY < contact.penetration)
			{
				contact.penetration = std::max(0.0f, overlapY);
				contact.normal = { 0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f };
			}
			if (overlapZ < contact.penetration)
			{
				contact.penetration = std::max(0.0f, overlapZ);
				contact.normal = { 0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f };
			}
			if (Vector3::LengthSquared(delta) <= 0.00000001f) contact.normal = { 0.0f, 1.0f, 0.0f };
		}
	}

	void PhysicsWorld::RegisterCollider(Collider* collider)
	{
		if (!collider || ContainsPointer(colliders_, collider)) return;
		colliders_.push_back(collider); // nullptrと重複登録を避け、外部所有のCollider参照だけを保持する。
	}

	void PhysicsWorld::UnregisterCollider(Collider* collider)
	{
		ErasePointer(colliders_, collider);
		contacts_.erase(
			std::remove_if(contacts_.begin(), contacts_.end(),
				[collider](const Contact& contact)
				{
					return contact.colliderA == collider || contact.colliderB == collider;
				}),
			contacts_.end());
		eventDispatcher_.Clear(); // 古いContactやイベント履歴が次ステップへ残らないように掃除する。
	}

	void PhysicsWorld::ClearColliders()
	{
		colliders_.clear();
		contacts_.clear();
		eventDispatcher_.Clear();
	}

	void PhysicsWorld::RegisterRigidbody(Rigidbody* rigidbody)
	{
		if (!rigidbody || ContainsPointer(rigidbodies_, rigidbody)) return;
		rigidbodies_.push_back(rigidbody);
	}

	void PhysicsWorld::UnregisterRigidbody(Rigidbody* rigidbody)
	{
		ErasePointer(rigidbodies_, rigidbody);
	}

	void PhysicsWorld::Update(float deltaTime)
	{
		const float clampedDeltaTime = std::clamp(deltaTime, 0.0f, maxDeltaTime_);
		lastSubStepCount_ = 0;
		if (!useFixedStep_)
		{
			accumulator_ = 0.0f;
			Step(clampedDeltaTime);
			lastSubStepCount_ = 1;
			return;
		}

		accumulator_ += clampedDeltaTime;
		while (accumulator_ >= fixedTimeStep_ && lastSubStepCount_ < maxSubSteps_)
		{
			Step(fixedTimeStep_);
			accumulator_ -= fixedTimeStep_;
			++lastSubStepCount_;
		}
		if (lastSubStepCount_ >= maxSubSteps_ && accumulator_ >= fixedTimeStep_) accumulator_ = 0.0f;
	}

	void PhysicsWorld::Step(float deltaTime)
	{
		contacts_.clear();
		ClearRigidbodyFrameState();
		IntegrateBodies(deltaTime);
		ClampRigidbodyVelocities();
		IntegrateColliderPositions(deltaTime);
		DetectCollisions();
		ResolveContacts();
		UpdateRigidbodySleepState(deltaTime);
		eventDispatcher_.Update(contacts_);
	}

	void PhysicsWorld::SetUseFixedStep(bool useFixedStep)
	{
		useFixedStep_ = useFixedStep;
		accumulator_ = 0.0f;
		lastSubStepCount_ = 0;
	}

	void PhysicsWorld::SetFixedTimeStep(float fixedTimeStep)
	{
		fixedTimeStep_ = std::clamp(fixedTimeStep, 1.0f / 240.0f, 1.0f / 15.0f);
		accumulator_ = std::min(accumulator_, fixedTimeStep_);
	}

	void PhysicsWorld::SetMaxDeltaTime(float maxDeltaTime)
	{
		maxDeltaTime_ = std::clamp(maxDeltaTime, 0.016f, 0.5f);
	}

	void PhysicsWorld::SetMaxSubSteps(int maxSubSteps)
	{
		maxSubSteps_ = std::clamp(maxSubSteps, 1, 16);
	}

	void PhysicsWorld::ApplySettings(const PhysicsWorldSettings& settings)
	{
		SetUseFixedStep(settings.useFixedStep);
		SetFixedTimeStep(settings.fixedTimeStep);
		SetMaxDeltaTime(settings.maxDeltaTime);
		SetMaxSubSteps(settings.maxSubSteps);
		gravity_ = settings.gravity;
		positionSolveEnabled_ = settings.enablePositionSolver;
		velocitySolveEnabled_ = settings.enableVelocitySolver;
		frictionSolveEnabled_ = settings.enableFrictionSolver;
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (!rigidbody) continue;
			rigidbody->SetGravity(gravity_);
			rigidbody->SetSleepEnabled(settings.enableSleep);
		}
	}

	PhysicsWorldSettings PhysicsWorld::GetSettings() const
	{
		PhysicsWorldSettings settings{};
		settings.useFixedStep = useFixedStep_;
		settings.fixedTimeStep = fixedTimeStep_;
		settings.maxDeltaTime = maxDeltaTime_;
		settings.maxSubSteps = maxSubSteps_;
		settings.gravity = gravity_;
		settings.enablePositionSolver = positionSolveEnabled_;
		settings.enableVelocitySolver = velocitySolveEnabled_;
		settings.enableFrictionSolver = frictionSolveEnabled_;
		return settings;
	}

	void PhysicsWorld::IntegrateBodies(float deltaTime)
	{
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (!rigidbody) continue;
			rigidbody->SetGravity(gravity_);
			rigidbody->Integrate(deltaTime);
		}
	}

	void PhysicsWorld::IntegrateColliderPositions(float deltaTime)
	{
		if (deltaTime <= 0.0f) return;
		for (Collider* collider : colliders_)
		{
			if (!collider) continue;
			Rigidbody* rigidbody = collider->GetRigidbody();
			if (!rigidbody || rigidbody->GetBodyType() != BodyType::Dynamic) continue;
			collider->SetCenterPosition(collider->GetCenterPosition() + rigidbody->GetVelocity() * deltaTime);
		}
	}

	void PhysicsWorld::DetectCollisions()
	{
		for (size_t i = 0; i < colliders_.size(); ++i)
		{
			for (size_t j = i + 1; j < colliders_.size(); ++j)
			{
				Collider* colliderA = colliders_[i];
				Collider* colliderB = colliders_[j];
				if (!colliderA || !colliderB) continue;
				if (!colliderA->IsCollisionEnabledForQuery() || !colliderB->IsCollisionEnabledForQuery()) continue;
				const CollisionResponseType response = responseMatrix_.GetResponse(colliderA->GetCollisionLayer(), colliderB->GetCollisionLayer());
				if (response == CollisionResponseType::Ignore || !TestCollisionPair(colliderA, colliderB)) continue;
				contacts_.push_back(BuildContact(colliderA, colliderB, response));
			}
		}
	}

	void PhysicsWorld::ResolveContacts()
	{
		PositionSolver positionSolver{};
		VelocitySolver velocitySolver{};
		FrictionSolver frictionSolver{};
		for (Contact& contact : contacts_)
		{
			if (positionSolveEnabled_) positionSolver.Resolve(contact);
			if (velocitySolveEnabled_) velocitySolver.Resolve(contact);
			if (frictionSolveEnabled_) frictionSolver.Resolve(contact);
			UpdateGroundedState(contact);
		}
	}

	void PhysicsWorld::ClearRigidbodyFrameState()
	{
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (rigidbody) rigidbody->ClearFrameState();
		}
	}

	void PhysicsWorld::UpdateRigidbodySleepState(float deltaTime)
	{
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (rigidbody) rigidbody->UpdateSleepState(deltaTime);
		}
	}

	void PhysicsWorld::UpdateGroundedState(const Contact& contact) const
	{
		if (contact.isTrigger || !contact.colliderA || !contact.colliderB) return;
		Rigidbody* rigidbodyA = contact.colliderA->GetRigidbody();
		Rigidbody* rigidbodyB = contact.colliderB->GetRigidbody();
		constexpr float kGroundNormalThreshold = 0.5f;
		if (rigidbodyA && rigidbodyA->GetBodyType() == BodyType::Dynamic && contact.normal.y < -kGroundNormalThreshold) rigidbodyA->SetGrounded(true);
		if (rigidbodyB && rigidbodyB->GetBodyType() == BodyType::Dynamic && contact.normal.y > kGroundNormalThreshold) rigidbodyB->SetGrounded(true);
	}

	bool PhysicsWorld::TestCollisionPair(Collider* colliderA, Collider* colliderB) const
	{
		switch (colliderA->GetShapeType())
		{
		case ECollisionShapeType::Sphere:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere: return CollisionUtility::IsCollision(colliderA->GetSphere(), colliderB->GetSphere());
			case ECollisionShapeType::AABB: return CollisionUtility::IsCollision(colliderB->GetAABB(), colliderA->GetSphere());
			case ECollisionShapeType::OBB: return CollisionUtility::IsCollision(colliderB->GetOBB(), colliderA->GetSphere());
			case ECollisionShapeType::Capsule: return CollisionUtility::IsCollision(colliderB->GetCapsule(), colliderA->GetSphere());
			default: return false;
			}
		case ECollisionShapeType::AABB:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere: return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetSphere());
			case ECollisionShapeType::AABB: return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetAABB());
			case ECollisionShapeType::OBB: return CollisionUtility::IsCollision(colliderB->GetOBB(), colliderA->GetAABB());
			case ECollisionShapeType::Capsule: return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetCapsule());
			case ECollisionShapeType::Segment: return CollisionUtility::IsCollision(colliderA->GetAABB(), colliderB->GetSegment());
			default: return false;
			}
		case ECollisionShapeType::OBB:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere: return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetSphere());
			case ECollisionShapeType::AABB: return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetAABB());
			case ECollisionShapeType::OBB: return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetOBB());
			case ECollisionShapeType::Segment: return CollisionUtility::IsCollision(colliderA->GetOBB(), colliderB->GetSegment());
			default: return false;
			}
		case ECollisionShapeType::Capsule:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::Sphere: return CollisionUtility::IsCollision(colliderA->GetCapsule(), colliderB->GetSphere());
			case ECollisionShapeType::AABB: return CollisionUtility::IsCollision(colliderB->GetAABB(), colliderA->GetCapsule());
			case ECollisionShapeType::Capsule: return CollisionUtility::IsCollision(colliderA->GetCapsule(), colliderB->GetCapsule());
			case ECollisionShapeType::Segment: return CollisionUtility::IsCollision(colliderA->GetCapsule(), colliderB->GetSegment());
			default: return false;
			}
		case ECollisionShapeType::Segment:
			switch (colliderB->GetShapeType())
			{
			case ECollisionShapeType::AABB: return CollisionUtility::IsCollision(colliderB->GetAABB(), colliderA->GetSegment());
			case ECollisionShapeType::OBB: return CollisionUtility::IsCollision(colliderB->GetOBB(), colliderA->GetSegment());
			case ECollisionShapeType::Capsule: return CollisionUtility::IsCollision(colliderB->GetCapsule(), colliderA->GetSegment());
			default: return false;
			}
		case ECollisionShapeType::None:
		default:
			return false;
		}
	}

	Contact PhysicsWorld::BuildContact(Collider* colliderA, Collider* colliderB, CollisionResponseType response) const
	{
		Contact contact{};
		contact.colliderA = colliderA;
		contact.colliderB = colliderB;
		if (IsBoxShape(colliderA->GetShapeType()) && IsBoxShape(colliderB->GetShapeType()))
		{
			const OBB boxA = BuildContactBox(*colliderA);
			const OBB boxB = BuildContactBox(*colliderB);
			if (!BuildBoxContactData(boxA, boxB, contact.normal, contact.penetration, contact.point)) BuildFallbackAabbContact(colliderA, colliderB, contact);
		}
		else
		{
			BuildFallbackAabbContact(colliderA, colliderB, contact);
		}
		contact.isTrigger = response == CollisionResponseType::Trigger || colliderA->IsTrigger() || colliderB->IsTrigger();
		return contact;
	}

	void PhysicsWorld::ClampRigidbodyVelocities()
	{
		constexpr float kMaxFallSpeed = -100.0f;
		constexpr float kMaxMoveSpeed = 100.0f;
		for (Rigidbody* rigidbody : rigidbodies_)
		{
			if (!rigidbody || rigidbody->GetBodyType() != BodyType::Dynamic) continue;
			Vector3 velocity = rigidbody->GetVelocity();
			if (velocity.y < kMaxFallSpeed) velocity.y = kMaxFallSpeed;
			velocity.x = std::clamp(velocity.x, -kMaxMoveSpeed, kMaxMoveSpeed);
			velocity.z = std::clamp(velocity.z, -kMaxMoveSpeed, kMaxMoveSpeed);
			rigidbody->SetVelocity(velocity);
		}
	}
} // namespace Ken4lowEngine
