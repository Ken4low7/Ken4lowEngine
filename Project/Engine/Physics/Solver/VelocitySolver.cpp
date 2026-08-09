#include "VelocitySolver.h"

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
			// 速度補正はDynamicだけを対象にし、Static/Kinematicは動かないBodyとして扱う。
			return rigidbody && rigidbody->GetBodyType() == BodyType::Dynamic;
		}

		float GetSafeRestitution(const Rigidbody* rigidbody)
		{
			// Rigidbodyが無い側は反発なしとして扱い、値は念のため0.0〜1.0へ丸める。
			return rigidbody ? std::clamp(rigidbody->GetRestitution(), 0.0f, 1.0f) : 0.0f;
		}
	}

	void VelocitySolver::Resolve(Contact& contact) const
	{
		// 接触面へ入り込む速度を補正する。TriggerやCollider未設定のContactは速度応答しない。
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
		const float velocityAlongNormal = Vector3::Dot(relativeVelocity, contact.normal);

		// normal方向に離れている、または静止している場合は速度補正を行わない。
		if (velocityAlongNormal >= 0.0f)
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

		const float restitution = std::max(GetSafeRestitution(rigidbodyA), GetSafeRestitution(rigidbodyB));
		const float impulseScalar = -(1.0f + restitution) * velocityAlongNormal / invMassSum;
		const Vector3 impulse = contact.normal * impulseScalar;

		if (dynamicA)
		{
			// Aはnormalの反対方向へ速度を補正する。
			rigidbodyA->SetVelocity(velocityA - impulse * invMassA);
		}
		if (dynamicB)
		{
			// Bはnormal方向へ速度を補正する。
			rigidbodyB->SetVelocity(velocityB + impulse * invMassB);
		}
	}

} // namespace Ken4lowEngine
