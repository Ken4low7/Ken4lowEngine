#pragma once
#include "Actor.h"
#include "ColliderComponent.h"
#include "RigidbodyComponent.h"
#include "SceneComponent.h"
#include "WaterSurfaceComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <string>
#include <unordered_map>
#include <utility>

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
		float submergedFraction = 0.0f;
		std::uint32_t submergedProbeCount = 0;
		std::uint32_t probeCount = 0;
	};

	struct WaterSplashEvent
	{
		Actor* actor = nullptr;
		Vector3 worldPosition{};
		Vector3 worldNormal{ 0.0f, 1.0f, 0.0f };
		float impactSpeed = 0.0f;
		float intensity = 0.0f;
		bool entering = true;
	};

	struct WaterInteractionDiagnostics
	{
		Actor* actor = nullptr;
		EWaterContactState state = EWaterContactState::AboveSurface;
		float waterHeight = 0.0f;
		float mass = 0.0f;
		float objectVolume = 0.0f;
		float submergedVolume = 0.0f;
		float submergedFraction = 0.0f;
		float gravityForce = 0.0f;
		float buoyancyForce = 0.0f;
		float buoyancyTorque = 0.0f;
		float dragForce = 0.0f;
		float angularSpeed = 0.0f;
		std::uint32_t probeCount = 0;
		std::uint32_t submergedProbeCount = 0;
	};

	class WaterInteractionComponent final : public ColliderComponent
	{
	public:
		using WaterContactCallback = std::function<void(const WaterContact&)>;
		using WaterSplashCallback = std::function<void(const WaterSplashEvent&)>;

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
			EvaluateTrackedContacts(deltaTime);
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
			diagnosticsValid_ = false;
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
			outJson["BuoyancyEnabled"] = buoyancyEnabled_;
			outJson["BuoyancyScale"] = buoyancyScale_;
			outJson["WaterDensity"] = waterDensity_;
			outJson["WaterLinearDrag"] = waterLinearDrag_;
			outJson["WaterAngularDrag"] = waterAngularDrag_;
			outJson["MultiPointSampling"] = multiPointSampling_;
			outJson["SurfaceAlignEnabled"] = surfaceAlignEnabled_;
			outJson["SurfaceAlignSpeed"] = surfaceAlignSpeed_;
			outJson["MaxTiltDegrees"] = maxTiltDegrees_;
			outJson["SplashEnabled"] = splashEnabled_;
			outJson["MinSplashSpeed"] = minSplashSpeed_;
			outJson["SplashIntensityScale"] = splashIntensityScale_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ColliderComponent::FromJson(inJson);
			autoFitSurface_ = inJson.value("AutoFitSurface", autoFitSurface_);
			volumeDepth_ = inJson.value("VolumeDepth", volumeDepth_);
			surfacePadding_ = inJson.value("SurfacePadding", surfacePadding_);
			surfaceTolerance_ = inJson.value("SurfaceTolerance", surfaceTolerance_);
			buoyancyEnabled_ = inJson.value("BuoyancyEnabled", buoyancyEnabled_);
			buoyancyScale_ = inJson.value("BuoyancyScale", buoyancyScale_);
			waterDensity_ = inJson.value("WaterDensity", waterDensity_);
			waterLinearDrag_ = inJson.value("WaterLinearDrag", waterLinearDrag_);
			waterAngularDrag_ = inJson.value("WaterAngularDrag", waterAngularDrag_);
			multiPointSampling_ = inJson.value("MultiPointSampling", multiPointSampling_);
			surfaceAlignEnabled_ = inJson.value("SurfaceAlignEnabled", surfaceAlignEnabled_);
			surfaceAlignSpeed_ = inJson.value("SurfaceAlignSpeed", surfaceAlignSpeed_);
			maxTiltDegrees_ = inJson.value("MaxTiltDegrees", maxTiltDegrees_);
			splashEnabled_ = inJson.value("SplashEnabled", splashEnabled_);
			minSplashSpeed_ = inJson.value("MinSplashSpeed", minSplashSpeed_);
			splashIntensityScale_ = inJson.value("SplashIntensityScale", splashIntensityScale_);
			SetShapeType(ECollisionShapeType::OBB);
			SetIsTrigger(true);
			SetCollisionTag("WaterVolume");
		}

		void DrawImGui() override
		{
			ColliderComponent::DrawImGui();
#ifdef USE_IMGUI
			// W4の浮力・入水判定をInspectorだけで確認できるよう、診断項目まで日本語へ統一する。
			ImGui::SeparatorText("水との相互作用");
			ImGui::Checkbox("水面へ自動フィット##WaterInteraction", &autoFitSurface_);
			ImGui::DragFloat("水深##WaterInteraction", &volumeDepth_, 0.1f, 0.1f, 100.0f);
			ImGui::DragFloat("水面上の余白##WaterInteraction", &surfacePadding_, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("接触許容幅##WaterInteraction", &surfaceTolerance_, 0.001f, 0.0f, 1.0f);

			ImGui::SeparatorText("浮力");
			ImGui::Checkbox("浮力を有効化##WaterInteraction", &buoyancyEnabled_);
			ImGui::DragFloat("浮力倍率##WaterInteraction", &buoyancyScale_, 0.01f, 0.0f, 5.0f);
			ImGui::DragFloat("水の密度 kg/m^3##WaterInteraction", &waterDensity_, 1.0f, 0.0f, 5000.0f);
			ImGui::DragFloat("水中の直線抵抗##WaterInteraction", &waterLinearDrag_, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("水中の回転抵抗##WaterInteraction", &waterAngularDrag_, 0.05f, 0.0f, 20.0f);
			ImGui::Checkbox("複数浮力点##WaterInteraction", &multiPointSampling_);
			ImGui::Checkbox("波面へ傾きを追従##WaterInteraction", &surfaceAlignEnabled_);
			ImGui::DragFloat("傾き追従速度##WaterInteraction", &surfaceAlignSpeed_, 0.05f, 0.0f, 20.0f);
			ImGui::DragFloat("最大傾斜角(度)##WaterInteraction", &maxTiltDegrees_, 0.5f, 0.0f, 60.0f);

			ImGui::SeparatorText("水しぶきイベント");
			ImGui::Checkbox("水しぶきイベントを有効化##WaterInteraction", &splashEnabled_);
			ImGui::DragFloat("発生する最低速度##WaterInteraction", &minSplashSpeed_, 0.05f, 0.0f, 50.0f);
			ImGui::DragFloat("水しぶき強度倍率##WaterInteraction", &splashIntensityScale_, 0.05f, 0.0f, 5.0f);

			ImGui::SeparatorText("診断");
			ImGui::Text("水面: %s", waterSurface_ ? "検出済み" : "未検出");
			ImGui::Text("接触候補数: %d", static_cast<int>(trackedColliders_.size()));
			ImGui::Text("入水中: %d", static_cast<int>(GetInWaterCount()));
			ImGui::Text("浮力対象数: %d", static_cast<int>(GetBuoyantBodyCount()));
			ImGui::Text("平均水没率: %.2f", GetAverageSubmergedFraction());
			ImGui::Text("直近の水しぶき強度: %.2f", lastSplashIntensity_);
			if (diagnosticsValid_)
			{
				ImGui::SeparatorText("直近の動的ボディ");
				ImGui::Text("水面高さ: %.3f", lastDiagnostics_.waterHeight);
				ImGui::Text("質量: %.3f kg", lastDiagnostics_.mass);
				ImGui::Text("物体体積: %.4f m^3", lastDiagnostics_.objectVolume);
				ImGui::Text("水没体積: %.4f m^3", lastDiagnostics_.submergedVolume);
				ImGui::Text("水没率: %.3f", lastDiagnostics_.submergedFraction);
				ImGui::Text("浮力点数: %u", lastDiagnostics_.probeCount);
				ImGui::Text("水没中の浮力点数: %u", lastDiagnostics_.submergedProbeCount);
				ImGui::Text("重力: %.3f N", lastDiagnostics_.gravityForce);
				ImGui::Text("浮力: %.3f N", lastDiagnostics_.buoyancyForce);
				ImGui::Text("浮力トルク: %.3f N*m", lastDiagnostics_.buoyancyTorque);
				ImGui::Text("抵抗力: %.3f N", lastDiagnostics_.dragForce);
				ImGui::Text("角速度: %.3f rad/s", lastDiagnostics_.angularSpeed);
				ImGui::Text("接触状態: %s", ContactStateName(lastDiagnostics_.state));
			}
			if (trackedColliders_.empty())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "候補0: 対象ActorにColliderComponentが必要です。");
			}
			ImGui::TextDisabled("1エンジン単位 = 1 m を前提にCollider体積から排水量を計算します。");
			ImGui::TextDisabled("複数浮力点では各浮力点へ力を分配し、r x Fでトルクを生成します。");
#endif
			SyncVolumeToSurface();
		}

		void SetOnWaterEnter(WaterContactCallback callback) { onWaterEnter_ = std::move(callback); }
		void SetOnWaterStay(WaterContactCallback callback) { onWaterStay_ = std::move(callback); }
		void SetOnWaterExit(WaterContactCallback callback) { onWaterExit_ = std::move(callback); }
		void SetOnWaterSplash(WaterSplashCallback callback) { onWaterSplash_ = std::move(callback); }

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

		float GetActorSubmergedFraction(const Actor* actor) const
		{
			if (!actor) return 0.0f;
			float fraction = 0.0f;
			for (const auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (tracked.inWater && tracked.lastContact.actor == actor)
				{
					fraction = (std::max)(fraction, tracked.lastContact.submergedFraction);
				}
			}
			return fraction;
		}

		std::size_t GetCandidateCount() const { return trackedColliders_.size(); }
		const WaterInteractionDiagnostics& GetLastDiagnostics() const { return lastDiagnostics_; }
		bool HasDiagnostics() const { return diagnosticsValid_; }

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
		struct ProbeSet
		{
			std::array<Vector3, 8> points{};
			std::size_t count = 0;
		};

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

			if (it->second.inWater)
			{
				if (onWaterExit_) onWaterExit_(it->second.lastContact);
				TryEmitSplash(it->second.lastContact, false);
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

		void EvaluateTrackedContacts(float deltaTime)
		{
			diagnosticsValid_ = false;
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
					TryEmitSplash(contact, true);
				}
				else if (isInWater && tracked.inWater)
				{
					if (onWaterStay_) onWaterStay_(contact);
				}
				else if (!isInWater && tracked.inWater)
				{
					if (onWaterExit_) onWaterExit_(contact);
					TryEmitSplash(contact, false);
				}

				if (isInWater && IsPrimaryTrackedColliderForActor(tracked.collider))
				{
					ApplyWaterDynamics(contact, deltaTime);
				}

				tracked.inWater = isInWater;
				tracked.lastContact = contact;
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

			const float diameter = (std::max)(contact.projectedExtent * 2.0f, 0.001f);
			const float geometricFraction = std::clamp(contact.submersionDepth / diameter, 0.0f, 1.0f);
			const ProbeSet probes = BuildProbeSet(collider);
			contact.probeCount = static_cast<std::uint32_t>(probes.count);

			Vector3 normalSum{};
			Vector3 surfacePositionSum{};
			float probeWeightSum = 0.0f;
			const float probeBand = (std::max)(surfaceTolerance_, 0.05f);
			for (std::size_t index = 0; index < probes.count; ++index)
			{
				const WaterSurfaceSample probeSample = waterSurface_->SampleSurfaceAtWorldPosition(probes.points[index]);
				const float weight = std::clamp(0.5f - probeSample.signedDistance / (probeBand * 2.0f), 0.0f, 1.0f);
				if (weight > 0.5f) ++contact.submergedProbeCount;
				probeWeightSum += weight;
				normalSum += probeSample.worldNormal * weight;
				surfacePositionSum += probeSample.worldPosition * weight;
			}

			const float probeFraction = probes.count > 0
				? std::clamp(probeWeightSum / static_cast<float>(probes.count), 0.0f, 1.0f)
				: geometricFraction;
			contact.submergedFraction = multiPointSampling_
				? std::clamp((probeFraction + geometricFraction) * 0.5f, 0.0f, 1.0f)
				: geometricFraction;

			if (probeWeightSum > 0.001f)
			{
				contact.surface.worldNormal = Vector3::NormalizeSafe(normalSum, contact.surface.worldNormal);
				contact.surface.worldPosition = surfacePositionSum / probeWeightSum;
			}

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

		ProbeSet BuildProbeSet(const Collider* collider) const
		{
			ProbeSet probes{};
			if (!collider) return probes;

			const Vector3 center = collider->GetCenterPosition();
			if (!multiPointSampling_)
			{
				probes.points[0] = center;
				probes.count = 1;
				return probes;
			}

			switch (collider->GetShapeType())
			{
			case ECollisionShapeType::AABB:
				{
					const AABB aabb = collider->GetAABB();
					for (int x = 0; x < 2; ++x)
					{
						for (int y = 0; y < 2; ++y)
						{
							for (int z = 0; z < 2; ++z)
							{
								probes.points[probes.count++] = {
									x == 0 ? aabb.min.x : aabb.max.x,
									y == 0 ? aabb.min.y : aabb.max.y,
									z == 0 ? aabb.min.z : aabb.max.z,
								};
							}
						}
					}
					break;
				}
			case ECollisionShapeType::OBB:
				{
					const OBB obb = collider->GetOBB();
					for (int x = -1; x <= 1; x += 2)
					{
						for (int y = -1; y <= 1; y += 2)
						{
							for (int z = -1; z <= 1; z += 2)
							{
								probes.points[probes.count++] = obb.center
									+ obb.orientations[0] * (obb.size.x * static_cast<float>(x))
									+ obb.orientations[1] * (obb.size.y * static_cast<float>(y))
									+ obb.orientations[2] * (obb.size.z * static_cast<float>(z));
							}
						}
					}
					break;
				}
			case ECollisionShapeType::Sphere:
				{
					const Sphere sphere = collider->GetSphere();
					const float radius = (std::max)(sphere.radius * 0.75f, 0.001f);
					probes.points[probes.count++] = sphere.center;
					probes.points[probes.count++] = sphere.center + Vector3{ radius, 0.0f, 0.0f };
					probes.points[probes.count++] = sphere.center + Vector3{ -radius, 0.0f, 0.0f };
					probes.points[probes.count++] = sphere.center + Vector3{ 0.0f, radius, 0.0f };
					probes.points[probes.count++] = sphere.center + Vector3{ 0.0f, -radius, 0.0f };
					probes.points[probes.count++] = sphere.center + Vector3{ 0.0f, 0.0f, radius };
					probes.points[probes.count++] = sphere.center + Vector3{ 0.0f, 0.0f, -radius };
					break;
				}
			case ECollisionShapeType::Capsule:
				{
					const Capsule capsule = collider->GetCapsule();
					const Vector3 capsuleEnd = capsule.segment.origin + capsule.segment.diff;
					const Vector3 capsuleCenter = capsule.GetCenter();
					const float radius = (std::max)(capsule.radius * 0.75f, 0.001f);
					probes.points[probes.count++] = capsule.segment.origin;
					probes.points[probes.count++] = capsuleEnd;
					probes.points[probes.count++] = capsuleCenter + Vector3{ radius, 0.0f, 0.0f };
					probes.points[probes.count++] = capsuleCenter + Vector3{ -radius, 0.0f, 0.0f };
					probes.points[probes.count++] = capsuleCenter + Vector3{ 0.0f, radius, 0.0f };
					probes.points[probes.count++] = capsuleCenter + Vector3{ 0.0f, -radius, 0.0f };
					probes.points[probes.count++] = capsuleCenter + Vector3{ 0.0f, 0.0f, radius };
					probes.points[probes.count++] = capsuleCenter + Vector3{ 0.0f, 0.0f, -radius };
					break;
				}
			default:
				probes.points[0] = center;
				probes.count = 1;
				break;
			}
			return probes;
		}

		void ApplyWaterDynamics(const WaterContact& contact, float deltaTime)
		{
			if (!contact.actor || !contact.collider || contact.submergedFraction <= 0.0f) return;
			RigidbodyComponent* rigidbodyComponent = contact.actor->GetComponent<RigidbodyComponent>();
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			if (!rigidbody || rigidbody->GetBodyType() != BodyType::Dynamic) return;

			const float submerged = std::clamp(contact.submergedFraction, 0.0f, 1.0f);
			const float safeDeltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
			const float objectVolume = CalculateColliderVolume(contact.collider);
			const float submergedVolume = objectVolume * submerged;
			float gravityMagnitude = Vector3::Length(rigidbody->GetGravity());
			if (gravityMagnitude <= 0.001f) gravityMagnitude = 9.8f;

			Vector3 buoyancyForce{};
			Vector3 buoyancyTorque{};
			if (buoyancyEnabled_ && submergedVolume > 0.0f)
			{
				const float buoyancyMagnitude = (std::max)(waterDensity_, 0.0f) * gravityMagnitude * submergedVolume * (std::max)(buoyancyScale_, 0.0f);
				const ProbeSet probes = BuildProbeSet(contact.collider);
				const Vector3 centerOfMass = contact.collider->GetCenterPosition();

				if (multiPointSampling_ && probes.count > 1)
				{
					std::array<WaterSurfaceSample, 8> probeSamples{};
					std::array<float, 8> probeWeights{};
					float probeWeightSum = 0.0f;
					const float probeBand = (std::max)(surfaceTolerance_, 0.05f);

					for (std::size_t index = 0; index < probes.count; ++index)
					{
						probeSamples[index] = waterSurface_->SampleSurfaceAtWorldPosition(probes.points[index]);
						probeWeights[index] = std::clamp(0.5f - probeSamples[index].signedDistance / (probeBand * 2.0f), 0.0f, 1.0f);
						probeWeightSum += probeWeights[index];
					}

					if (probeWeightSum > 0.001f)
					{
						for (std::size_t index = 0; index < probes.count; ++index)
						{
							if (probeWeights[index] <= 0.0f) continue;
							const float forceShare = probeWeights[index] / probeWeightSum;
							const Vector3 probeForce = probeSamples[index].worldNormal * (buoyancyMagnitude * forceShare);
							rigidbody->AddForceAtPosition(probeForce, probes.points[index], centerOfMass);
							buoyancyForce += probeForce;
							buoyancyTorque += Vector3::Cross(probes.points[index] - centerOfMass, probeForce);
						}
					}
					else
					{
						buoyancyForce = contact.surface.worldNormal * buoyancyMagnitude;
						rigidbody->AddForce(buoyancyForce);
					}
				}
				else
				{
					buoyancyForce = contact.surface.worldNormal * buoyancyMagnitude;
					rigidbody->AddForce(buoyancyForce);
				}
			}

			Vector3 dragForce{};
			if (waterLinearDrag_ > 0.0f && safeDeltaTime > 0.0f)
			{
				const Vector3 velocityBeforeDrag = rigidbody->GetVelocity();
				const float damping = std::exp(-(std::max)(waterLinearDrag_, 0.0f) * submerged * safeDeltaTime);
				const Vector3 velocityAfterDrag = velocityBeforeDrag * damping;
				dragForce = (velocityAfterDrag - velocityBeforeDrag) * (rigidbody->GetMass() / safeDeltaTime);
				rigidbody->SetVelocity(velocityAfterDrag);
			}

			if (waterAngularDrag_ > 0.0f && safeDeltaTime > 0.0f)
			{
				const Vector3 angularVelocity = rigidbody->GetAngularVelocity();
				const float angularDamping = std::exp(-(std::max)(waterAngularDrag_, 0.0f) * submerged * safeDeltaTime);
				rigidbody->SetAngularVelocity(angularVelocity * angularDamping);
			}

			if (surfaceAlignEnabled_)
			{
				ApplySurfaceAlignmentTorque(contact.actor, rigidbody, contact.surface.worldNormal, submerged);
			}

			lastDiagnostics_.actor = contact.actor;
			lastDiagnostics_.state = contact.state;
			lastDiagnostics_.waterHeight = contact.surface.worldPosition.y;
			lastDiagnostics_.mass = rigidbody->GetMass();
			lastDiagnostics_.objectVolume = objectVolume;
			lastDiagnostics_.submergedVolume = submergedVolume;
			lastDiagnostics_.submergedFraction = submerged;
			lastDiagnostics_.gravityForce = rigidbody->GetMass() * gravityMagnitude;
			lastDiagnostics_.buoyancyForce = Vector3::Length(buoyancyForce);
			lastDiagnostics_.buoyancyTorque = Vector3::Length(buoyancyTorque);
			lastDiagnostics_.dragForce = Vector3::Length(dragForce);
			lastDiagnostics_.angularSpeed = Vector3::Length(rigidbody->GetAngularVelocity());
			lastDiagnostics_.probeCount = contact.probeCount;
			lastDiagnostics_.submergedProbeCount = contact.submergedProbeCount;
			diagnosticsValid_ = true;
		}

		void ApplySurfaceAlignmentTorque(Actor* actor, Rigidbody* rigidbody, const Vector3& surfaceNormal, float submergedFraction) const
		{
			if (!actor || !rigidbody || submergedFraction <= 0.0f) return;
			SceneComponent* root = actor->GetRootComponent();
			if (!root || root->GetParent()) return;

			const Vector3 normal = Vector3::NormalizeSafe(surfaceNormal, { 0.0f, 1.0f, 0.0f });
			const float maxTiltRadians = std::clamp(maxTiltDegrees_, 0.0f, 60.0f) * (std::numbers::pi_v<float> / 180.0f);
			const float safeY = (std::max)(normal.y, 0.001f);
			const float targetPitch = std::clamp(std::atan2(normal.z, safeY), -maxTiltRadians, maxTiltRadians);
			const float targetRoll = std::clamp(-std::atan2(normal.x, safeY), -maxTiltRadians, maxTiltRadians);
			const Vector3 currentRotation = root->GetLocalRotation();
			const Vector3 angularVelocity = rigidbody->GetAngularVelocity();
			const float response = (std::max)(surfaceAlignSpeed_, 0.0f);
			const float stiffness = response * response * submergedFraction;
			const float damping = response * 2.0f * submergedFraction;
			const Vector3 desiredAngularAcceleration{
				(targetPitch - currentRotation.x) * stiffness - angularVelocity.x * damping,
				0.0f,
				(targetRoll - currentRotation.z) * stiffness - angularVelocity.z * damping
			};
			const Vector3 invInertia = rigidbody->GetInvInertia();
			Vector3 alignmentTorque{};
			if (invInertia.x > 0.000001f) alignmentTorque.x = desiredAngularAcceleration.x / invInertia.x;
			if (invInertia.z > 0.000001f) alignmentTorque.z = desiredAngularAcceleration.z / invInertia.z;
			rigidbody->AddTorque(alignmentTorque); // 波面追従もTransform直書きではなくTorqueとして物理系へ渡す。
		}

		void TryEmitSplash(const WaterContact& contact, bool entering)
		{
			if (!splashEnabled_ || !onWaterSplash_ || !contact.actor) return;
			RigidbodyComponent* rigidbodyComponent = contact.actor->GetComponent<RigidbodyComponent>();
			Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
			if (!rigidbody) return;

			const Vector3 velocity = rigidbody->GetVelocity();
			const float normalSpeed = Vector3::Dot(velocity, contact.surface.worldNormal);
			const float impactSpeed = entering ? (std::max)(-normalSpeed, 0.0f) : (std::max)(normalSpeed, 0.0f);
			if (impactSpeed < (std::max)(minSplashSpeed_, 0.0f)) return;

			WaterSplashEvent event{};
			event.actor = contact.actor;
			event.worldPosition = contact.surface.worldPosition;
			event.worldNormal = contact.surface.worldNormal;
			event.impactSpeed = impactSpeed;
			event.intensity = std::clamp((impactSpeed - minSplashSpeed_) * (std::max)(splashIntensityScale_, 0.0f), 0.0f, 1.0f);
			event.entering = entering;
			lastSplashIntensity_ = event.intensity;
			onWaterSplash_(event);
		}

		bool IsPrimaryTrackedColliderForActor(const Collider* collider) const
		{
			if (!collider) return false;
			Actor* actor = collider->GetOwner<Actor>();
			if (!actor) return false;
			const std::uint32_t candidateId = collider->GetUniqueID();
			for (const auto& [id, tracked] : trackedColliders_)
			{
				if (!tracked.collider || tracked.collider == collider) continue;
				if (tracked.collider->GetOwner<Actor>() == actor && id < candidateId) return false;
			}
			return true;
		}

		std::size_t GetBuoyantBodyCount() const
		{
			std::size_t count = 0;
			for (const auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (!tracked.inWater || !tracked.lastContact.actor) continue;
				RigidbodyComponent* rigidbodyComponent = tracked.lastContact.actor->GetComponent<RigidbodyComponent>();
				Rigidbody* rigidbody = rigidbodyComponent ? rigidbodyComponent->GetRigidbody() : nullptr;
				if (rigidbody && rigidbody->GetBodyType() == BodyType::Dynamic && IsPrimaryTrackedColliderForActor(tracked.collider)) ++count;
			}
			return count;
		}

		float GetAverageSubmergedFraction() const
		{
			float sum = 0.0f;
			std::size_t count = 0;
			for (const auto& [id, tracked] : trackedColliders_)
			{
				(void)id;
				if (!tracked.inWater) continue;
				sum += tracked.lastContact.submergedFraction;
				++count;
			}
			return count > 0 ? sum / static_cast<float>(count) : 0.0f;
		}

		static float CalculateColliderVolume(const Collider* collider)
		{
			if (!collider) return 0.0f;
			constexpr float fourThirds = 4.0f / 3.0f;
			constexpr float pi = std::numbers::pi_v<float>;

			switch (collider->GetShapeType())
			{
			case ECollisionShapeType::AABB:
				{
					const AABB aabb = collider->GetAABB();
					const Vector3 size = aabb.max - aabb.min;
					return (std::max)(size.x, 0.0f) * (std::max)(size.y, 0.0f) * (std::max)(size.z, 0.0f);
				}
			case ECollisionShapeType::OBB:
				{
					const Vector3 halfSize = collider->GetOBB().size;
					return 8.0f * (std::max)(halfSize.x, 0.0f) * (std::max)(halfSize.y, 0.0f) * (std::max)(halfSize.z, 0.0f);
				}
			case ECollisionShapeType::Sphere:
				{
					const float radius = (std::max)(collider->GetSphere().radius, 0.0f);
					return fourThirds * pi * radius * radius * radius;
				}
			case ECollisionShapeType::Capsule:
				{
					const Capsule capsule = collider->GetCapsule();
					const float radius = (std::max)(capsule.radius, 0.0f);
					const float cylinderLength = (std::max)(Vector3::Length(capsule.segment.diff), 0.0f);
					return pi * radius * radius * cylinderLength + fourThirds * pi * radius * radius * radius;
				}
			default:
				return 0.0f;
			}
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
			case ECollisionShapeType::Capsule:
				{
					const Capsule capsule = collider->GetCapsule();
					return std::fabs(Vector3::Dot(normal, capsule.segment.diff)) * 0.5f + (std::max)(capsule.radius, 0.0f);
				}
			default:
				return 0.0f;
			}
		}

		static const char* ContactStateName(EWaterContactState state)
		{
			switch (state)
			{
			case EWaterContactState::AboveSurface:
				return "水面より上";
			case EWaterContactState::TouchingSurface:
				return "水面に接触";
			case EWaterContactState::Submerged:
				return "水没";
			default:
				return "不明";
			}
		}

		WaterSurfaceComponent* waterSurface_ = nullptr;
		std::unordered_map<std::uint32_t, TrackedCollider> trackedColliders_;
		WaterContactCallback onWaterEnter_{};
		WaterContactCallback onWaterStay_{};
		WaterContactCallback onWaterExit_{};
		WaterSplashCallback onWaterSplash_{};
		WaterInteractionDiagnostics lastDiagnostics_{};
		float volumeDepth_ = 5.0f;
		float surfacePadding_ = 0.5f;
		float surfaceTolerance_ = 0.02f;
		float buoyancyScale_ = 1.0f;
		float waterDensity_ = 1000.0f;
		float waterLinearDrag_ = 2.5f;
		float waterAngularDrag_ = 1.5f;
		float surfaceAlignSpeed_ = 3.0f;
		float maxTiltDegrees_ = 20.0f;
		float minSplashSpeed_ = 1.0f;
		float splashIntensityScale_ = 0.25f;
		float lastSplashIntensity_ = 0.0f;
		bool autoFitSurface_ = true;
		bool buoyancyEnabled_ = true;
		bool multiPointSampling_ = true;
		bool surfaceAlignEnabled_ = true;
		bool splashEnabled_ = true;
		bool diagnosticsValid_ = false;
	};
} // namespace Ken4lowEngine