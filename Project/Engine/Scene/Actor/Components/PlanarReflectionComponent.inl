#include "Actor.h"
#include "DirectXCommon.h"
#include "Matrix4x4.h"
#include "Plane.h"
#include "Wireframe.h"

#include <algorithm>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace PlanarReflectionComponentDetail
	{
		inline const char* UpdateModeToString(PlanarReflectionUpdateMode updateMode)
		{
			return updateMode == PlanarReflectionUpdateMode::OnDemand ? "OnDemand" : "EveryFrame";
		}

		inline PlanarReflectionUpdateMode UpdateModeFromString(const std::string& value)
		{
			return value == "OnDemand" ? PlanarReflectionUpdateMode::OnDemand : PlanarReflectionUpdateMode::EveryFrame;
		}
	}

	inline void PlanarReflectionComponent::Initialize()
	{
		SceneComponent::Initialize();
		PlanarReflectionManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
		SyncToManager(true); // 配置直後の最初のMirror Captureを必ず予約する。
	}

	inline void PlanarReflectionComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		SyncToManager();
	}

	inline void PlanarReflectionComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		SyncToManager();
	}

	inline void PlanarReflectionComponent::Draw()
	{
		if (!enabled_ || !debugPlaneVisible_ || !IsActiveInHierarchy()) return;
		Wireframe* wireframe = Wireframe::GetInstance();
		if (!Wireframe::IsDebugDrawSupported() || !wireframe->IsDebugDrawEnabled()) return;
		const Vector3 normal = GetPlaneNormal();
		Plane plane{};
		plane.normal = normal;
		plane.distance = Vector3::Dot(normal, GetWorldPosition());
		wireframe->DrawPlane(plane, (std::max)(debugPlaneSize_, 0.1f), { 0.15f, 0.95f, 0.90f, 1.0f });
		wireframe->DrawLine(
			GetWorldPosition(),
			GetWorldPosition() + normal * (std::max)(debugPlaneSize_ * 0.35f, 0.25f),
			{ 1.0f, 0.35f, 0.15f, 1.0f }); // 鏡面法線を表示し、壁鏡/床鏡の向きをEditor上で確認できるようにする。
	}

	inline void PlanarReflectionComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("Planar Reflection");
		bool changed = false;
		changed |= ImGui::Checkbox("有効##PlanarReflectionEnabled", &enabled_);
		changed |= ImGui::SliderFloat("反射強度##PlanarReflectionStrength", &strength_, 0.0f, 1.0f, "%.2f");

		const char* updateModeNames[] = { "On Demand", "Every Frame" };
		int updateModeIndex = updateMode_ == PlanarReflectionUpdateMode::OnDemand ? 0 : 1;
		if (ImGui::Combo("更新モード##PlanarReflectionUpdateMode", &updateModeIndex, updateModeNames, IM_ARRAYSIZE(updateModeNames)))
		{
			updateMode_ = updateModeIndex == 0 ? PlanarReflectionUpdateMode::OnDemand : PlanarReflectionUpdateMode::EveryFrame;
			changed = true;
		}

		changed |= ImGui::Checkbox("法線を反転##PlanarReflectionFlipNormal", &flipNormal_);
		changed |= ImGui::Checkbox("平面を表示##PlanarReflectionDebugPlane", &debugPlaneVisible_);
		changed |= ImGui::DragFloat("デバッグ平面サイズ##PlanarReflectionDebugSize", &debugPlaneSize_, 0.1f, 0.1f, 100.0f);
		strength_ = std::clamp(strength_, 0.0f, 1.0f);
		debugPlaneSize_ = (std::max)(debugPlaneSize_, 0.1f);

		if (changed)
		{
			SyncToManager();
		}
		if (ImGui::Button("再キャプチャ##PlanarReflectionRecapture", ImVec2(-1.0f, 0.0f)))
		{
			RequestCapture();
		}

		const PlanarReflectionDiagnostics diagnostics = PlanarReflectionManager::GetInstance()->GetDiagnostics(this);
		ImGui::Text("状態: %s", diagnostics.captured ? (diagnostics.dirty ? "再Capture待ち" : "Captured") : "未Capture");
		ImGui::Text("Capture Revision: %llu", static_cast<unsigned long long>(diagnostics.captureRevision));
		ImGui::TextDisabled("同じActorのModelComponentが鏡面Receiverになります。");
		ImGui::TextDisabled("Local +Yが鏡面法線です。Planeモデルを推奨します。");
		ImGui::TextDisabled("v1はLegacy Material専用です。PBR MaterialではPlanar反射を適用しません。");
		if (updateMode_ == PlanarReflectionUpdateMode::OnDemand)
		{
			ImGui::TextDisabled("On DemandはCamera移動後に再キャプチャが必要です。");
		}
		else
		{
			ImGui::TextDisabled("Every Frameは正確な鏡用ですがSceneを追加描画するため高負荷です。");
		}
#endif
	}

	inline void PlanarReflectionComponent::Finalize()
	{
		PlanarReflectionManager::GetInstance()->UnregisterSurface(this);
	}

	inline void PlanarReflectionComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		outJson["Enabled"] = enabled_;
		outJson["Strength"] = strength_;
		outJson["UpdateMode"] = PlanarReflectionComponentDetail::UpdateModeToString(updateMode_);
		outJson["FlipNormal"] = flipNormal_;
		outJson["DebugPlaneVisible"] = debugPlaneVisible_;
		outJson["DebugPlaneSize"] = debugPlaneSize_;
	}

	inline void PlanarReflectionComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		if (const auto it = inJson.find("Enabled"); it != inJson.end() && it->is_boolean()) enabled_ = it->get<bool>();
		if (const auto it = inJson.find("Strength"); it != inJson.end() && it->is_number()) strength_ = it->get<float>();
		if (const auto it = inJson.find("UpdateMode"); it != inJson.end() && it->is_string()) updateMode_ = PlanarReflectionComponentDetail::UpdateModeFromString(it->get<std::string>());
		if (const auto it = inJson.find("FlipNormal"); it != inJson.end() && it->is_boolean()) flipNormal_ = it->get<bool>();
		if (const auto it = inJson.find("DebugPlaneVisible"); it != inJson.end() && it->is_boolean()) debugPlaneVisible_ = it->get<bool>();
		if (const auto it = inJson.find("DebugPlaneSize"); it != inJson.end() && it->is_number()) debugPlaneSize_ = it->get<float>();
		strength_ = std::clamp(strength_, 0.0f, 1.0f);
		debugPlaneSize_ = (std::max)(debugPlaneSize_, 0.1f);
	}

	inline void PlanarReflectionComponent::SyncToManager(bool forceDirty)
	{
		RefreshWorldTransform();
		PlanarReflectionManager::GetInstance()->RegisterOrUpdateSurface(
			this,
			GetOwner(),
			BuildDesc(),
			forceDirty);
	}

	inline void PlanarReflectionComponent::RequestCapture()
	{
		SyncToManager();
		PlanarReflectionManager::GetInstance()->RequestCapture(this);
	}

	inline void PlanarReflectionComponent::SetStrength(float strength)
	{
		strength_ = std::clamp(strength, 0.0f, 1.0f);
	}

	inline Vector3 PlanarReflectionComponent::GetPlaneNormal() const
	{
		const Matrix4x4 rotation = Matrix4x4::MakeRotateMatrix(GetWorldRotation());
		Vector3 normal = Vector3::NormalizeSafe(
			Vector3::Transform({ 0.0f, 1.0f, 0.0f }, rotation),
			{ 0.0f, 1.0f, 0.0f });
		if (flipNormal_)
		{
			normal = -normal;
		}
		return normal;
	}

	inline PlanarReflectionDesc PlanarReflectionComponent::BuildDesc() const
	{
		PlanarReflectionDesc desc{};
		desc.position = GetWorldPosition();
		desc.normal = GetPlaneNormal();
		desc.strength = strength_;
		desc.updateMode = updateMode_;
		desc.enabled = enabled_ && IsActiveInHierarchy();
		return desc;
	}
} // namespace Ken4lowEngine
