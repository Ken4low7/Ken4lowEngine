#include "ReflectionProbeComponent.h"

#include "Wireframe.h"

#include <algorithm>
#include <array>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace
	{
		const char* UpdateModeToString(ReflectionProbeUpdateMode updateMode)
		{
			switch (updateMode)
			{
			case ReflectionProbeUpdateMode::Static: return "Static";
			case ReflectionProbeUpdateMode::OnDemand: return "OnDemand";
			case ReflectionProbeUpdateMode::EveryFrame: return "EveryFrame";
			default: return "Static";
			}
		}

		ReflectionProbeUpdateMode UpdateModeFromString(const std::string& value)
		{
			if (value == "OnDemand") return ReflectionProbeUpdateMode::OnDemand;
			if (value == "EveryFrame") return ReflectionProbeUpdateMode::EveryFrame;
			return ReflectionProbeUpdateMode::Static;
		}

		uint32_t SanitizeProbeResolution(uint32_t resolution)
		{
			constexpr std::array<uint32_t, 4> supported{ 64, 128, 256, 512 };
			uint32_t best = supported.front();
			uint32_t bestDistance = UINT32_MAX;
			for (uint32_t candidate : supported)
			{
				const uint32_t distance = candidate > resolution ? candidate - resolution : resolution - candidate;
				if (distance < bestDistance)
				{
					best = candidate;
					bestDistance = distance;
				}
			}
			return best;
		}
	}

	void ReflectionProbeComponent::Initialize()
	{
		SceneComponent::Initialize();
		Sanitize();
		SyncToManager(true); // 初回だけ必ずCapture対象にして、配置直後から局所反射を利用できるようにする。
	}

	void ReflectionProbeComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		SyncToManager();
	}

	void ReflectionProbeComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		SyncToManager(); // Editor Gizmoで移動したProbeもPlayせず次Captureへ反映する。
	}

	void ReflectionProbeComponent::Draw()
	{
		if (!enabled_ || !debugBoundsVisible_ || !IsActiveInHierarchy()) return;
		Wireframe* wireframe = Wireframe::GetInstance();
		if (!Wireframe::IsDebugDrawSupported() || !wireframe->IsDebugDrawEnabled()) return;
		wireframe->DrawSphere(GetWorldPosition(), influenceRadius_, { 0.20f, 0.85f, 1.0f, 1.0f });
	}

	void ReflectionProbeComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("Reflection Probe");
		bool changed = false;
		changed |= ImGui::Checkbox("有効##ReflectionProbeEnabled", &enabled_);

		const char* updateModes[] = { "Static", "On Demand", "Every Frame" };
		int updateModeIndex = std::clamp(static_cast<int>(updateMode_), 0, 2);
		if (ImGui::Combo("更新モード##ReflectionProbeUpdateMode", &updateModeIndex, updateModes, IM_ARRAYSIZE(updateModes)))
		{
			updateMode_ = static_cast<ReflectionProbeUpdateMode>(updateModeIndex);
			changed = true;
		}

		changed |= ImGui::DragFloat("影響半径##ReflectionProbeRadius", &influenceRadius_, 0.1f, 0.1f, 1000.0f);
		changed |= ImGui::DragFloat("Near Clip##ReflectionProbeNear", &nearClip_, 0.01f, 0.01f, 10.0f);
		changed |= ImGui::DragFloat("Far Clip##ReflectionProbeFar", &farClip_, 1.0f, 1.0f, 10000.0f);

		constexpr uint32_t resolutions[] = { 64, 128, 256, 512 };
		int resolutionIndex = 0;
		for (int index = 0; index < 4; ++index)
		{
			if (resolutions[index] == resolution_) resolutionIndex = index;
		}
		const char* resolutionNames[] = { "64", "128", "256", "512" };
		if (ImGui::Combo("解像度##ReflectionProbeResolution", &resolutionIndex, resolutionNames, IM_ARRAYSIZE(resolutionNames)))
		{
			resolution_ = resolutions[resolutionIndex];
			changed = true;
		}

		changed |= ImGui::Checkbox("影響範囲を表示##ReflectionProbeDebugBounds", &debugBoundsVisible_);
		Sanitize();
		if (changed)
		{
			SyncToManager();
		}

		if (ImGui::Button("再キャプチャ##ReflectionProbeRecapture", ImVec2(-1.0f, 0.0f)))
		{
			RequestCapture();
		}

		const ReflectionProbeDiagnostics diagnostics = ReflectionProbeManager::GetInstance()->GetDiagnostics(this);
		ImGui::Text("状態: %s", diagnostics.captured ? (diagnostics.dirty ? "再Capture待ち" : "Captured") : "未Capture");
		ImGui::Text("Capture Revision: %llu", static_cast<unsigned long long>(diagnostics.captureRevision));
		ImGui::TextDisabled("Static/On Demandは必要時のみ更新し、Every Frameは高負荷です。");
#endif
	}

	void ReflectionProbeComponent::Finalize()
	{
		ReflectionProbeManager::GetInstance()->UnregisterProbe(this);
	}

	void ReflectionProbeComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		outJson["Enabled"] = enabled_;
		outJson["InfluenceRadius"] = influenceRadius_;
		outJson["NearClip"] = nearClip_;
		outJson["FarClip"] = farClip_;
		outJson["Resolution"] = resolution_;
		outJson["UpdateMode"] = UpdateModeToString(updateMode_);
		outJson["DebugBoundsVisible"] = debugBoundsVisible_;
	}

	void ReflectionProbeComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		if (const auto it = inJson.find("Enabled"); it != inJson.end() && it->is_boolean()) enabled_ = it->get<bool>();
		if (const auto it = inJson.find("InfluenceRadius"); it != inJson.end() && it->is_number()) influenceRadius_ = it->get<float>();
		if (const auto it = inJson.find("NearClip"); it != inJson.end() && it->is_number()) nearClip_ = it->get<float>();
		if (const auto it = inJson.find("FarClip"); it != inJson.end() && it->is_number()) farClip_ = it->get<float>();
		if (const auto it = inJson.find("Resolution"); it != inJson.end() && it->is_number_unsigned()) resolution_ = it->get<uint32_t>();
		if (const auto it = inJson.find("UpdateMode"); it != inJson.end() && it->is_string()) updateMode_ = UpdateModeFromString(it->get<std::string>());
		if (const auto it = inJson.find("DebugBoundsVisible"); it != inJson.end() && it->is_boolean()) debugBoundsVisible_ = it->get<bool>();
		Sanitize();
	}

	void ReflectionProbeComponent::SyncToManager(bool forceDirty)
	{
		RefreshWorldTransform();
		ReflectionProbeManager::GetInstance()->RegisterOrUpdateProbe(this, BuildDesc(), forceDirty);
	}

	void ReflectionProbeComponent::RequestCapture()
	{
		SyncToManager();
		ReflectionProbeManager::GetInstance()->RequestCapture(this);
	}

	void ReflectionProbeComponent::SetInfluenceRadius(float radius)
	{
		influenceRadius_ = std::max(radius, 0.1f);
	}

	void ReflectionProbeComponent::SetNearClip(float nearClip)
	{
		nearClip_ = std::max(nearClip, 0.01f);
		farClip_ = std::max(farClip_, nearClip_ + 0.1f);
	}

	void ReflectionProbeComponent::SetFarClip(float farClip)
	{
		farClip_ = std::max(farClip, nearClip_ + 0.1f);
	}

	void ReflectionProbeComponent::SetResolution(uint32_t resolution)
	{
		resolution_ = SanitizeProbeResolution(resolution);
	}

	ReflectionProbeDesc ReflectionProbeComponent::BuildDesc() const
	{
		ReflectionProbeDesc desc{};
		desc.position = GetWorldPosition();
		desc.influenceRadius = influenceRadius_;
		desc.nearClip = nearClip_;
		desc.farClip = farClip_;
		desc.resolution = resolution_;
		desc.updateMode = updateMode_;
		desc.enabled = enabled_ && IsActiveInHierarchy();
		return desc;
	}

	void ReflectionProbeComponent::Sanitize()
	{
		influenceRadius_ = std::max(influenceRadius_, 0.1f);
		nearClip_ = std::max(nearClip_, 0.01f);
		farClip_ = std::max(farClip_, nearClip_ + 0.1f);
		resolution_ = SanitizeProbeResolution(resolution_);
	}
} // namespace Ken4lowEngine
