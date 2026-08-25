#pragma once

#include "SceneComponent.h"
#include "Engine/Graphics/Renderer/Reflection/PlanarReflectionManager.h"

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	class ModelComponent;

	enum class PlanarReflectionFacePreset : uint8_t
	{
		PositiveX = 0,
		NegativeX,
		PositiveY,
		NegativeY,
		PositiveZ,
		NegativeZ,
	};

	/// <summary>
	/// 同じActorのModelComponentを鏡面Receiverとして扱うPlanar Reflection Componentです。
	/// 1 Componentを1枚の鏡面として扱い、単一鏡面ではReceiver形状の最薄軸から法線を自動判定します。
	/// 同一Actorへ複数追加した場合は従来の面プリセットを使い、最大6面まで独立した鏡面を構築できます。
	/// </summary>
	class PlanarReflectionComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void Draw() override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override { return "PlanarReflectionComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SyncToManager(bool forceDirty = false);
		void RequestCapture();

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void SetStrength(float strength);
		float GetStrength() const { return strength_; }
		void SetUpdateMode(PlanarReflectionUpdateMode updateMode) { updateMode_ = updateMode; }
		PlanarReflectionUpdateMode GetUpdateMode() const { return updateMode_; }
		void SetQuality(PlanarReflectionQuality quality) { quality_ = quality; }
		PlanarReflectionQuality GetQuality() const { return quality_; }
		void SetFlipNormal(bool flip) { flipNormal_ = flip; }
		bool IsNormalFlipped() const { return flipNormal_; }
		void SetAutoDetectReceiverNormal(bool enabled)
		{
			autoDetectReceiverNormal_ = enabled;
			InvalidateAutoNormalCache();
		}
		bool IsAutoDetectReceiverNormalEnabled() const { return autoDetectReceiverNormal_; }
		void SetAutoFitToReceiverSurface(bool enabled) { autoFitToReceiverSurface_ = enabled; }
		bool IsAutoFitToReceiverSurfaceEnabled() const { return autoFitToReceiverSurface_; }
		void SetPlaneOffset(float offset) { planeOffset_ = offset; }
		float GetPlaneOffset() const { return planeOffset_; }
		void SetSurfaceTolerance(float tolerance);
		float GetSurfaceTolerance() const { return surfaceTolerance_; }
		void SetClipPlaneBias(float bias);
		float GetClipPlaneBias() const { return clipPlaneBias_; }
		void SetFacePreset(PlanarReflectionFacePreset preset);
		Vector3 GetPlaneNormal() const;
		Vector3 GetPlanePosition() const;

	private:
		bool TryResolveAutoPlaneNormal(Vector3& outNormal) const;
		void InvalidateAutoNormalCache() const
		{
			autoNormalCacheValid_ = false;
			autoNormalHasResolvedAxis_ = false;
			autoNormalReceiver_ = nullptr;
			autoNormalReceiverRevision_ = 0;
		}
		PlanarReflectionDesc BuildDesc() const;

		bool enabled_ = true;
		float strength_ = 1.0f;
		PlanarReflectionUpdateMode updateMode_ = PlanarReflectionUpdateMode::EveryFrame;
		PlanarReflectionQuality quality_ = PlanarReflectionQuality::Ultra;
		bool flipNormal_ = false;
		bool autoDetectReceiverNormal_ = true;
		bool autoFitToReceiverSurface_ = true;
		float planeOffset_ = 0.0f;
		float surfaceTolerance_ = 0.025f;
		float clipPlaneBias_ = 0.01f;
		bool debugPlaneVisible_ = true;
		float debugPlaneSize_ = 2.0f;

		mutable bool autoNormalCacheValid_ = false;
		mutable bool autoNormalHasResolvedAxis_ = false;
		mutable const ModelComponent* autoNormalReceiver_ = nullptr;
		mutable std::uint64_t autoNormalReceiverRevision_ = 0;
		mutable Vector3 autoNormalAxis_{ 0.0f, 1.0f, 0.0f };
		mutable float autoNormalCenterProjection_ = 0.0f;
	};
} // namespace Ken4lowEngine

#ifdef max
#pragma push_macro("max")
#undef max
#define KEN4LOW_PLANAR_REFLECTION_COMPONENT_RESTORE_MAX
#endif

#ifdef min
#pragma push_macro("min")
#undef min
#define KEN4LOW_PLANAR_REFLECTION_COMPONENT_RESTORE_MIN
#endif

#include "PlanarReflectionComponent.inl" // ImGui値のclampをWindowsマクロから保護する。

#ifdef KEN4LOW_PLANAR_REFLECTION_COMPONENT_RESTORE_MIN
#pragma pop_macro("min")
#undef KEN4LOW_PLANAR_REFLECTION_COMPONENT_RESTORE_MIN
#endif

#ifdef KEN4LOW_PLANAR_REFLECTION_COMPONENT_RESTORE_MAX
#pragma pop_macro("max")
#undef KEN4LOW_PLANAR_REFLECTION_COMPONENT_RESTORE_MAX
#endif