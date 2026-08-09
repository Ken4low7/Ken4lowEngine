#include "PositionSolver.h"

#include "Collider.h"
#include "PhysicsTypes.h"
#include "Rigidbody.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsMovableBody(const Rigidbody* rigidbody)
		{
			// Dynamicは質量比で、KinematicはStaticとのめり込み解消時だけ位置補正対象にする。
			return rigidbody &&
				(rigidbody->GetBodyType() == BodyType::Dynamic || rigidbody->GetBodyType() == BodyType::Kinematic);
		}

		float GetPositionInvMass(const Rigidbody* rigidbody)
		{
			if (!rigidbody || rigidbody->GetBodyType() != BodyType::Dynamic)
			{
				return 0.0f; // Static/KinematicはDynamicから見て無限質量として扱う。
			}
			return std::max(rigidbody->GetInvMass(), 0.0f);
		}

		void MoveCollider(Collider* collider, const Vector3& correction)
		{
			// Colliderの中心だけを補正し、速度反射や摩擦などの応答は次Phaseへ残す。
			if (!collider)
			{
				return;
			}
			collider->SetCenterPosition(collider->GetCenterPosition() + correction);
		}
	}

	void PositionSolver::Resolve(Contact& contact) const
	{
		// Contact情報を使ってめり込みを補正する。penetrationが無い接触やTriggerは位置補正しない。
		if (contact.penetration <= 0.0f || contact.isTrigger || !contact.colliderA || !contact.colliderB)
		{
			return;
		}

		Rigidbody* rigidbodyA = contact.colliderA->GetRigidbody();
		Rigidbody* rigidbodyB = contact.colliderB->GetRigidbody();
		const bool movableA = IsMovableBody(rigidbodyA);
		const bool movableB = IsMovableBody(rigidbodyB);

		if (!movableA && !movableB)
		{
			return;
		}

		const Vector3 correction = contact.normal * contact.penetration;

		if (movableA && movableB)
		{
			const float invMassA = GetPositionInvMass(rigidbodyA);
			const float invMassB = GetPositionInvMass(rigidbodyB);
			const float invMassSum = invMassA + invMassB;

			if (invMassSum > 0.000001f)
			{
				// 軽いBodyほど大きく、重いBodyほど小さく押し戻してmassを位置解決にも反映する。
				const float weightA = invMassA / invMassSum;
				const float weightB = invMassB / invMassSum;
				MoveCollider(contact.colliderA, correction * -weightA);
				MoveCollider(contact.colliderB, correction * weightB);
				return;
			}

			// Kinematic同士だけは逆質量を持たないため、未解決のめり込みを残さないよう半分ずつ分ける。
			MoveCollider(contact.colliderA, correction * -0.5f);
			MoveCollider(contact.colliderB, correction * 0.5f);
			return;
		}

		if (movableA)
		{
			// normalはAからB方向なので、Aだけ動ける場合は反対方向へ全量押し戻す。
			MoveCollider(contact.colliderA, correction * -1.0f);
			return;
		}

		// Bだけ動ける場合はnormal方向へ全量押し戻す。
		MoveCollider(contact.colliderB, correction);
	}

} // namespace Ken4lowEngine
