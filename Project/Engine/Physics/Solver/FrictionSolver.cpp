#include "FrictionSolver.h"

#include "Collider.h"
#include "PhysicsTypes.h"
#include "Rigidbody.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsDynamicBody(const Rigidbody* rigidbody)
		{
			// 摩擦はDynamicだけを対象にし、Static/Kinematicは動かないBodyとして扱う。
			return rigidbody && rigidbody->GetBodyType() == BodyType::Dynamic;
		}

		float GetSafeDynamicFriction(const Rigidbody* rigidbody)
		{
			// Rigidbodyが無い側は摩擦なしとして扱い、値は負にならないようにする。
			return rigidbody ? std::max(rigidbody->GetDynamicFriction(), 0.0f) : 0.0f;
		}
	}

	void FrictionSolver::Resolve(Contact& contact) const
	{
		// 接触面に沿った速度を摩擦で減速させる。TriggerやCollider未設定のContactは摩擦応答しない。
		if (contact.isTrigger || !contact.colliderA || !contact.colliderB)
		{
			return;
		}

		Rigidbody* rigidbodyA = contact.colliderA->GetRigidbody();
		Rigidbody* rigidbodyB = contact.colliderB->GetRigidbody();
		const bool dynamicA = IsDynamicBody(rigidbodyA);
		const bool dynamicB = IsDynamicBody(rigidbodyB);
		if (!dynamicA && !dynamicB)
		{
			return;
		}

		const Vector3 velocityA = dynamicA ? rigidbodyA->GetVelocity() : Vector3{};
		const Vector3 velocityB = dynamicB ? rigidbodyB->GetVelocity() : Vector3{};
		const Vector3 relativeVelocity = velocityB - velocityA;
		const float normalSpeed = Vector3::Dot(relativeVelocity, contact.normal);
		const Vector3 tangentVelocity = relativeVelocity - contact.normal * normalSpeed;
		const float tangentSpeed = Vector3::Length(tangentVelocity);
		if (tangentSpeed <= 0.0001f)
		{
			return;
		}

		const float invMassA = dynamicA ? rigidbodyA->GetInvMass() : 0.0f;
		const float invMassB = dynamicB ? rigidbodyB->GetInvMass() : 0.0f;
		const float invMassSum = invMassA + invMassB;
		if (invMassSum <= 0.0f)
		{
			return;
		}

		const float friction = std::max(GetSafeDynamicFriction(rigidbodyA), GetSafeDynamicFriction(rigidbodyB));
		const float reductionRatio = std::clamp(friction, 0.0f, 1.0f);
		const Vector3 deltaRelativeVelocity = tangentVelocity * -reductionRatio;
		const Vector3 impulse = deltaRelativeVelocity / invMassSum;

		if (dynamicA)
		{
			// A側は相対速度を減らす向きへ速度を補正する。
			rigidbodyA->SetVelocity(velocityA - impulse * invMassA);
		}
		if (dynamicB)
		{
			// B側は摩擦で接線方向の滑り速度が反転しない範囲に収める。
			rigidbodyB->SetVelocity(velocityB + impulse * invMassB);
		}
	}

} // namespace Ken4lowEngine
