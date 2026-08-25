#pragma once
#include "Actor.h"
#include "ColliderComponent.h"
#include "WaterSurfaceComponent.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	enum class EWaterContactState : std::uint8_t
	{
		AboveSurface,
		TouchingSurface,
		Submerged,
	};

	struct WaterContact
	{
		Actor* actor = nullptr;
		Collider* collider = nullptr;
		WaterSurfaceSample surface{};
		EWaterContactState state = EWaterContactState::AboveSurface;
		float projectedExtent = 0.0f;
		float submersionDepth = 0.0f;
	};

	class WaterInteractionComponent final : public ColliderComponent
	{
	public:
		using WaterContactCallback = std::function<void(const WaterContact&)>;

		void Initialize() override
		{
			ResolveWaterSurface();
			SyncVolumeToSurface();
			SetShapeType(ECollisionShapeType::OBB);
			SetIsTrigger(true);
			SetCollisionTag("WaterVolume");
			ColliderComponent::Initialize();

			SetOnOverlapBeginCallback([this](const CollisionHit& hit) { HandleOverlapBegin(hit); });
			SetOnOverlapStayCallback([this](const CollisionHit& hit) { HandleOverlapStay(hit); });
			SetOnOverlapEndCallback([this](const CollisionHit& hit) { HandleOverlapEnd(hit); });
		}

		void Update(float deltaTime) override
		{
			ResolveWaterSurface();
			SyncVolumeToSurface();
			ColliderComponent::Update(deltaTime);
			EvaluateTrackedContacts();
		}

		void UpdateEditor(float deltaTime) override
		{
			ResolveWaterSurface();
			SyncVolumeToSurface();
			ColliderComponent::UpdateEditor(deltaTime);
		}

		void Finalize() override
		{
			trackedColliders_.clear();
			ClearCollisionCallbacks();
			ColliderComponent::Finalize();
		}

		std::string GetClassTypeName() const override
		{
			return "WaterInteractionComponent";
		}

		void ToJson(nlohmann::json& outJson) const override
		{
			ColliderComponent::ToJson(outJson);
			outJson["Class"] = GetClassTypeName();
			outJson["AutoFitSurface"] = autoFitSurface_;
			outJson["VolumeDepth"] = volumeDepth_;
			outJson["SurfacePadding"] = surfacePadding_;
			outJson["SurfaceTolerance"] = surfaceTolerance_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ColliderComponent::FromJson(inJson);
			autoFitSurface_ = inJson.value("AutoFitSurface", autoFitSurface_);
			volumeDepth_ = inJson.value("VolumeDepth", volumeDepth_);
			surfacePadding_ = inJson.value("SurfacePadding", surfacePadding_);
			surfaceTolerance_ = inJson.value("SurfaceTolerance", surfaceTolerance_);
			SetShapeType(ECollisionShapeType::OBB);
			SetIsTrigger(true);
			SetCollisionTag("WaterVolume");
		}

		void DrawImGui() override
		{
			ColliderComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("Water Interaction");
			ImGui::Checkbox("水面へ自動フィット##WaterInteraction", &autoFitSurface_);
			ImGui::DragFloat("水深##WaterInteraction", &volumeDepth_, 0.1f, 0.1f, 100.0f);
			ImGui::DragFloat("水面上の余白##WaterInteraction", &surfacePadding_, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("接触許容幅##WaterInteraction", &surfaceTolerance_, 0.001f, 0.0f, 1.0f);
			ImGui::Text("Water Surface: %s", waterSurface_ ? "Found" : "Missing");
			ImGui::Text("Candidates: %d", static_cast<int>(trackedColliders_.size()));
			ImGui::Text("In Water: %d", static_cast<int>(GetInWaterCount()));
			ImGui::TextDisabled("Triggerは候補抽出専用です。実際の入水判定はGerstner波面で行います。");
#endif
			SyncVolumeToSurface();
		}

		void SetOnWaterEnter(WaterContactCallback callback) { onWaterEnter_ = std::move(callback); }
		void SetOnWaterStay(WaterContactCallback callback) { onWaterStay_ = std::move(callback); }
		void SetOnWaterExit(WaterContactCallback callback) { onWaterExit_ = std::move(callback); }

		bool IsActorInWater(const Actor* actor) const
		{
			if (!actor) return false;
			for (const auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (tracked.inWater && tracked.lastContact.actor == actor) return true;
			}
			return false;
		}

		float GetActorSubmersionDepth(const Actor* actor) const
		{
			if (!actor) return 0.0f;
			float deepest = 0.0f;
			for (const auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (tracked.inWater && tracked.lastContact.actor == actor)
				{
					deepest = (std::max)(deepest, tracked.lastContact.submersionDepth);
				}
			}
			return deepest;
		}

		std::size_t GetCandidateCount() const { return trackedColliders_.size(); }

		std::size_t GetInWaterCount() const
		{
			std::size_t count = 0;
			for (const auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (tracked.inWater) ++count;
			}
			return count;
		}

	private:
		struct TrackedCollider
		{
			Collider* collider = nullptr;
			bool inWater = false;
			WaterContact lastContact{};
		};

		void ResolveWaterSurface()
		{
			Actor* owner = GetOwner();
			waterSurface_ = owner ? owner->GetComponent<WaterSurfaceComponent>() : nullptr;
		}

		void SyncVolumeToSurface()
		{
			SetShapeType(ECollisionShapeType::OBB);
			SetIsTrigger(true);
			SetCollisionTag("WaterVolume");
			if (!autoFitSurface_ || !waterSurface_) return;

			const Vector3 waterScale = waterSurface_->GetLocalScale();
			const Vector3 waterPosition = waterSurface_->GetLocalPosition();
			const float clampedDepth = (std::max)(volumeDepth_, 0.1f);
			const float clampedPadding = (std::max)(surfacePadding_, 0.0f);
			const float totalHeight = clampedDepth + clampedPadding;

			SetHalfSize({
				(std::max)(std::fabs(waterScale.x) * 0.5f, 0.1f),
				totalHeight * 0.5f,
				(std::max)(std::fabs(waterScale.y) * 0.5f, 0.1f)
			});
			SetLocalPosition({
				waterPosition.x,
				waterPosition.y + (clampedPadding - clampedDepth) * 0.5f,
				waterPosition.z
			});
		}

		void HandleOverlapBegin(const CollisionHit& hit)
		{
			TrackCollider(hit.other);
		}

		void HandleOverlapStay(const CollisionHit& hit)
		{
			TrackCollider(hit.other);
		}

		void HandleOverlapEnd(const CollisionHit& hit)
		{
			if (!hit.other) return;
			const auto it = trackedColliders_.find(hit.other->GetUniqueID());
			if (it == trackedColliders_.end()) return;

			if (it->second.inWater && onWaterExit_)
			{
				onWaterExit_(it->second.lastContact);
			}
			trackedColliders_.erase(it);
		}

		void TrackCollider(Collider* collider)
		{
			if (!collider) return;
			Actor* otherActor = collider->GetOwner<Actor>();
			if (!otherActor || otherActor == GetOwner()) return;

			TrackedCollider& tracked = trackedColliders_[collider->GetUniqueID()];
			tracked.collider = collider;
		}

		void EvaluateTrackedContacts()
		{
			if (!waterSurface_) return;

			for (auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (!tracked.collider) continue;

				WaterContact contact = BuildContact(tracked.collider);
				const bool isInWater = contact.state != EWaterContactState::AboveSurface;

				if (isInWater && !tracked.inWater)
				{
					if (onWaterEnter_) onWaterEnter_(contact);
				}
				else if (isInWater && tracked.inWater)
				{
					if (onWaterStay_) onWaterStay_(contact);
				}
				else if (!isInWater && tracked.inWater)
				{
					if (onWaterExit_) onWaterExit_(contact);
				}

				tracked.inWater = isInWater;
				tracked.lastContact = contact; // Triggerは候補抽出だけに使い、実際の入水状態はGerstner波面との距離で判定する。
			}
		}

		WaterContact BuildContact(Collider* collider) const
		{
			WaterContact contact{};
			if (!collider || !waterSurface_) return contact;

			contact.collider = collider;
			contact.actor = collider->GetOwner<Actor>();
			const Vector3 center = collider->GetCenterPosition();
			contact.surface = waterSurface_->SampleSurfaceAtWorldPosition(center);
			contact.projectedExtent = CalculateProjectedExtent(collider, contact.surface.worldNormal);

			const float lowestDistance = contact.surface.signedDistance - contact.projectedExtent;
			const float highestDistance = contact.surface.signedDistance + contact.projectedExtent;
			const float tolerance = (std::max)(surfaceTolerance_, 0.0f);
			contact.submersionDepth = (std::max)(-lowestDistance, 0.0f);

			if (lowestDistance > tolerance)
			{
				contact.state = EWaterContactState::AboveSurface;
			}
			else if (highestDistance < -tolerance)
			{
				contact.state = EWaterContactState::Submerged;
			}
			else
			{
				contact.state = EWaterContactState::TouchingSurface;
			}
			return contact;
		}

		static float CalculateProjectedExtent(const Collider* collider, const Vector3& normal)
		{
			if (!collider) return 0.0f;

			switch (collider->GetShapeType())
			{
			case ECollisionShapeType::Sphere:
				return (std::max)(collider->GetSphere().radius, 0.0f);
			case ECollisionShapeType::AABB:
				{
					const AABB aabb = collider->GetAABB();
					const Vector3 half = (aabb.max - aabb.min) * 0.5f;
					return std::fabs(normal.x) * half.x + std::fabs(normal.y) * half.y + std::fabs(normal.z) * half.z;
				}
			case ECollisionShapeType::OBB:
				{
					const OBB obb = collider->GetOBB();
					return std::fabs(Vector3::Dot(normal, obb.orientations[0])) * obb.size.x +
						std::fabs(Vector3::Dot(normal, obb.orientations[1])) * obb.size.y +
						std::fabs(Vector3::Dot(normal, obb.orientations[2])) * obb.size.z;
				}
			default:
				return 0.0f;
			}
		}

		WaterSurfaceComponent* waterSurface_ = nullptr;
		std::unordered_map<std::uint32_t, TrackedCollider> trackedColliders_;
		WaterContactCallback onWaterEnter_{};
		WaterContactCallback onWaterStay_{};
		WaterContactCallback onWaterExit_{};
		float volumeDepth_ = 5.0f;
		float surfacePadding_ = 0.5f;
		float surfaceTolerance_ = 0.02f;
		bool autoFitSurface_ = true;
	};
} // namespace Ken4lowEngine
