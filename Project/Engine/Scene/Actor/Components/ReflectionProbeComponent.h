#pragma once

#include "SceneComponent.h"
#include "Engine/Graphics/Renderer/Reflection/ReflectionProbeManager.h"

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>Actorの位置を中心に局所CubemapをCaptureするReflection Probe Componentです。</summary>
	class ReflectionProbeComponent : public SceneComponent
	{
	public:
		void Initialize() override;
		void Update(float deltaTime) override;
		void UpdateEditor(float deltaTime) override;
		void Draw() override;
		void DrawImGui() override;
		void Finalize() override;

		std::string GetClassTypeName() const override { return "ReflectionProbeComponent"; }
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		void SyncToManager(bool forceDirty = false);
		void RequestCapture();

		void SetEnabled(bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void SetInfluenceRadius(float radius);
		float GetInfluenceRadius() const { return influenceRadius_; }
		void SetNearClip(float nearClip);
		float GetNearClip() const { return nearClip_; }
		void SetFarClip(float farClip);
		float GetFarClip() const { return farClip_; }
		void SetResolution(uint32_t resolution);
		uint32_t GetResolution() const { return resolution_; }
		void SetUpdateMode(ReflectionProbeUpdateMode updateMode) { updateMode_ = updateMode; }
		ReflectionProbeUpdateMode GetUpdateMode() const { return updateMode_; }
		void SetDebugBoundsVisible(bool visible) { debugBoundsVisible_ = visible; }
		bool IsDebugBoundsVisible() const { return debugBoundsVisible_; }

	private:
		ReflectionProbeDesc BuildDesc() const;
		void Sanitize();

		bool enabled_ = true;
		float influenceRadius_ = 10.0f;
		float nearClip_ = 0.1f;
		float farClip_ = 100.0f;
		uint32_t resolution_ = 256;
		ReflectionProbeUpdateMode updateMode_ = ReflectionProbeUpdateMode::Static;
		bool debugBoundsVisible_ = true;
	};
} // namespace Ken4lowEngine

#include "ReflectionProbeComponent.inl"
