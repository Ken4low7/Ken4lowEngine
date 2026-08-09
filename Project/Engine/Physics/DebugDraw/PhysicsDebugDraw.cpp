#include "PhysicsDebugDraw.h"

#include "Collider.h"
#include "Contact.h"
#include "PhysicsTypes.h"
#include "PhysicsWorld.h"
#include "Rigidbody.h"
#include "PhysicsEvent.h"
#include "Wireframe.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void PhysicsDebugDraw::Draw(const PhysicsWorld& physicsWorld)
	{
#ifdef _DEBUG
		if (!settings_.drawPhysicsDebug)
		{
			return;
		}

		if (settings_.drawColliders)
		{
			std::size_t drawnColliderCount = 0;
			for (const Collider* collider : physicsWorld.GetColliders())
			{
				if (!collider)
				{
					continue;
				}
				if (drawnColliderCount >= settings_.maxColliderDrawCount)
				{
					break; // 大量のStage Colliderを毎フレーム全描画してEditorを重くしない。
				}

				DrawCollider(*collider, GetColliderColor(*collider));
				++drawnColliderCount;
			}
		}

		if (settings_.drawContacts)
		{
			for (const Contact& contact : physicsWorld.GetContacts())
			{
				DrawContact(contact);
			}
		}

		if (settings_.drawVelocity)
		{
			for (const Rigidbody* rigidbody : physicsWorld.GetRigidbodies())
			{
				if (rigidbody)
				{
					DrawVelocity(physicsWorld, *rigidbody);
				}
			}
		}
#else
		(void)physicsWorld;
#endif
	}

	void PhysicsDebugDraw::DrawImGui(const PhysicsWorld& physicsWorld)
	{
#ifdef USE_IMGUI
		if (ImGui::CollapsingHeader("PhysicsWorld Debug Draw", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Checkbox("Draw Physics Debug", &settings_.drawPhysicsDebug);
			ImGui::Checkbox("Draw Colliders", &settings_.drawColliders);
			ImGui::Checkbox("Draw Contacts", &settings_.drawContacts);
			ImGui::Checkbox("Draw Contact Normals", &settings_.drawContactNormals);
			ImGui::Checkbox("Draw Velocity", &settings_.drawVelocity);
			ImGui::Checkbox("Draw Sleeping", &settings_.drawSleeping);
			ImGui::Checkbox("Draw Events", &settings_.drawEvents);

			int maxColliderDrawCount = static_cast<int>((std::min)(settings_.maxColliderDrawCount, static_cast<std::size_t>(4096)));
			if (ImGui::DragInt("Max Collider Draw Count", &maxColliderDrawCount, 1.0f, 0, 4096))
			{
				settings_.maxColliderDrawCount = static_cast<std::size_t>((std::max)(maxColliderDrawCount, 0));
			}

			ImGui::DragFloat("Normal Length", &settings_.normalLength, 0.05f, 0.0f, 10.0f);
			ImGui::DragFloat("Velocity Scale", &settings_.velocityScale, 0.01f, 0.0f, 5.0f);

			ImGui::SeparatorText("PhysicsWorld State");
			ImGui::Text("Collider Count: %zu", physicsWorld.GetColliderCount());
			ImGui::Text("Collider Draw Limit: %zu", settings_.maxColliderDrawCount);
			ImGui::Text("Rigidbody Count: %zu", physicsWorld.GetRigidbodies().size());
			ImGui::Text("Contact Count: %zu", physicsWorld.GetContactCount());
			ImGui::Text("Event Count: %zu", physicsWorld.GetEvents().size());

			if (ImGui::TreeNode("Rigidbodies"))
			{
				const auto& rigidbodies = physicsWorld.GetRigidbodies();
				for (size_t i = 0; i < rigidbodies.size(); ++i)
				{
					const Rigidbody* rigidbody = rigidbodies[i];
					if (!rigidbody)
					{
						continue;
					}

					const Vector3 velocity = rigidbody->GetVelocity();
					ImGui::Text(
						"[%zu] %p Type=%s Grounded=%s Sleeping=%s Velocity=(%.3f, %.3f, %.3f)",
						i,
						static_cast<const void*>(rigidbody),
						ToBodyTypeName(rigidbody),
						rigidbody->IsGrounded() ? "true" : "false",
						rigidbody->IsSleeping() ? "true" : "false",
						velocity.x,
						velocity.y,
						velocity.z);
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Contacts"))
			{
				const auto& contacts = physicsWorld.GetContacts();
				for (size_t i = 0; i < contacts.size(); ++i)
				{
					const Contact& contact = contacts[i];
					ImGui::Text(
						"[%zu] %s A=%p B=%p Point=(%.3f, %.3f, %.3f) Normal=(%.3f, %.3f, %.3f) Penetration=%.3f",
						i,
						contact.isTrigger ? "Trigger" : "Block",
						static_cast<void*>(contact.colliderA),
						static_cast<void*>(contact.colliderB),
						contact.point.x,
						contact.point.y,
						contact.point.z,
						contact.normal.x,
						contact.normal.y,
						contact.normal.z,
						contact.penetration);
				}
				ImGui::TreePop();
			}

			if (settings_.drawEvents && ImGui::TreeNode("Events"))
			{
				const auto& events = physicsWorld.GetEvents();
				const size_t displayCount = std::min<size_t>(events.size(), 20u);
				for (size_t i = 0; i < displayCount; ++i)
				{
					const PhysicsEvent& event = events[events.size() - 1u - i];
					ImGui::Text(
						"[%zu] %s A=%p B=%p isTrigger=%s",
						i,
						ToEventTypeName(static_cast<int>(event.type)),
						static_cast<void*>(event.colliderA),
						static_cast<void*>(event.colliderB),
						event.isTrigger ? "true" : "false");
				}
				ImGui::TreePop();
			}
		}
#else
		(void)physicsWorld;
#endif
	}

	Vector4 PhysicsDebugDraw::GetColliderColor(const Collider& collider) const
	{
		const Rigidbody* rigidbody = collider.GetRigidbody();
		if (collider.IsTrigger())
		{
			return { 1.0f, 0.85f, 0.1f, 1.0f };
		}
		if (rigidbody && settings_.drawSleeping && rigidbody->IsSleeping())
		{
			return { 0.45f, 0.45f, 0.45f, 1.0f };
		}
		if (!rigidbody || rigidbody->GetBodyType() == BodyType::Static)
		{
			return { 0.2f, 0.75f, 1.0f, 1.0f };
		}
		if (rigidbody->GetBodyType() == BodyType::Kinematic)
		{
			return { 0.75f, 0.35f, 1.0f, 1.0f };
		}
		return { 0.2f, 1.0f, 0.35f, 1.0f };
	}

	void PhysicsDebugDraw::DrawCollider(const Collider& collider, const Vector4& color) const
	{
		Wireframe* wireframe = Wireframe::GetInstance();
		switch (collider.GetShapeType())
		{
		case ECollisionShapeType::Sphere:
			wireframe->DrawSphere(collider.GetSphere(), color);
			break;
		case ECollisionShapeType::OBB:
			wireframe->DrawOBB(collider.GetOBB(), color);
			break;
		case ECollisionShapeType::Capsule:
			wireframe->DrawCapsule(collider.GetCapsule(), color);
			break;
		case ECollisionShapeType::Segment:
			wireframe->DrawSegment(collider.GetSegment(), color);
			break;
		case ECollisionShapeType::AABB:
		default:
			wireframe->DrawAABB(collider.GetAABB(), color);
			break;
		}
	}

	void PhysicsDebugDraw::DrawContact(const Contact& contact) const
	{
		if (!contact.colliderA || !contact.colliderB)
		{
			return;
		}

		Wireframe* wireframe = Wireframe::GetInstance();
		const Vector4 color = contact.isTrigger ? Vector4{ 1.0f, 0.85f, 0.1f, 1.0f } : Vector4{ 1.0f, 0.15f, 0.1f, 1.0f };
		wireframe->DrawSphere(contact.point, 0.12f, color);
		if (settings_.drawContactNormals)
		{
			// Contact normalを描画して押し戻し方向を確認する。
			wireframe->DrawLine(contact.point, contact.point + contact.normal * settings_.normalLength, color);
		}
	}

	void PhysicsDebugDraw::DrawVelocity(const PhysicsWorld& physicsWorld, const Rigidbody& rigidbody) const
	{
		const Vector3 velocity = rigidbody.GetVelocity();
		if (Vector3::LengthSquared(velocity) <= 0.0001f)
		{
			return;
		}

		for (const Collider* collider : physicsWorld.GetColliders())
		{
			if (!collider || collider->GetRigidbody() != &rigidbody)
			{
				continue;
			}

			// Rigidbodyの速度方向を表示して物理挙動を確認する。
			const Vector3 start = collider->GetCenterPosition();
			Wireframe::GetInstance()->DrawLine(start, start + velocity * settings_.velocityScale, { 0.1f, 1.0f, 1.0f, 1.0f });
			return;
		}
	}

	const char* PhysicsDebugDraw::ToBodyTypeName(const Rigidbody* rigidbody) const
	{
		if (!rigidbody)
		{
			return "None";
		}

		switch (rigidbody->GetBodyType())
		{
		case BodyType::Static:
			return "Static";
		case BodyType::Kinematic:
			return "Kinematic";
		case BodyType::Dynamic:
			return "Dynamic";
		default:
			return "Unknown";
		}
	}

	const char* PhysicsDebugDraw::ToEventTypeName(int eventType) const
	{
		switch (static_cast<PhysicsEventType>(eventType))
		{
		case PhysicsEventType::CollisionEnter:
			return "CollisionEnter";
		case PhysicsEventType::CollisionStay:
			return "CollisionStay";
		case PhysicsEventType::CollisionExit:
			return "CollisionExit";
		case PhysicsEventType::TriggerEnter:
			return "TriggerEnter";
		case PhysicsEventType::TriggerStay:
			return "TriggerStay";
		case PhysicsEventType::TriggerExit:
			return "TriggerExit";
		default:
			return "Unknown";
		}
	}
} // namespace Ken4lowEngine
