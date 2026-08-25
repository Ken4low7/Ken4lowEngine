#include "Actor.h"
#include "DirectXCommon.h"
#include "Engine/Graphics/Renderer/Reflection/PlanarReflectionCaptureDiagnostics.h"
#include "Matrix4x4.h"
#include "ModelComponent.h"
#include "Plane.h"
#include "Wireframe.h"

#include <algorithm>
#include <limits>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	namespace PlanarReflectionComponentDetail
	{
		inline constexpr float kPi = 3.14159265358979323846f;
		inline constexpr float kHalfPi = kPi * 0.5f;

		inline const char* UpdateModeToString(PlanarReflectionUpdateMode updateMode)
		{
			return updateMode == PlanarReflectionUpdateMode::OnDemand ? "OnDemand" : "EveryFrame";
		}

		inline PlanarReflectionUpdateMode UpdateModeFromString(const std::string& value)
		{
			return value == "OnDemand" ? PlanarReflectionUpdateMode::OnDemand : PlanarReflectionUpdateMode::EveryFrame;
		}

		inline const char* QualityToString(PlanarReflectionQuality quality)
		{
			switch (quality)
			{
			case PlanarReflectionQuality::Low: return "Low";
			case PlanarReflectionQuality::Medium: return "Medium";
			case PlanarReflectionQuality::High: return "High";
			case PlanarReflectionQuality::Ultra:
			default:
				return "Ultra";
			}
		}

		inline PlanarReflectionQuality QualityFromString(const std::string& value)
		{
			if (value == "Low") return PlanarReflectionQuality::Low;
			if (value == "Medium") return PlanarReflectionQuality::Medium;
			if (value == "High") return PlanarReflectionQuality::High;
			return PlanarReflectionQuality::Ultra; // 旧Sceneや未知値は従来等倍品質へフォールバックする。
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
		const Vector3 planePosition = GetPlanePosition();
		Plane plane{};
		plane.normal = normal;
		plane.distance = Vector3::Dot(normal, planePosition);
		wireframe->DrawPlane(plane, (std::max)(debugPlaneSize_, 0.1f), { 0.15f, 0.95f, 0.90f, 1.0f });
		wireframe->DrawLine(
			planePosition,
			planePosition + normal * (std::max)(debugPlaneSize_ * 0.35f, 0.25f),
			{ 1.0f, 0.35f, 0.15f, 1.0f }); // Auto Fit後の実際の鏡面位置と法線をEditor上で確認できるようにする。
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

		const char* qualityNames[] = { "Low (25%)", "Medium (50%)", "High (75%)", "Ultra (100%)" };
		int qualityIndex = static_cast<int>(quality_);
		if (ImGui::Combo("反射品質##PlanarReflectionQuality", &qualityIndex, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			quality_ = static_cast<PlanarReflectionQuality>(qualityIndex);
			changed = true;
		}

		ImGui::TextDisabled("面方向プリセット（Local +Yを指定方向へ向けます）");
		if (ImGui::Button("+X##PlanarReflectionFace")) SetFacePreset(PlanarReflectionFacePreset::PositiveX);
		ImGui::SameLine();
		if (ImGui::Button("-X##PlanarReflectionFace")) SetFacePreset(PlanarReflectionFacePreset::NegativeX);
		ImGui::SameLine();
		if (ImGui::Button("+Y##PlanarReflectionFace")) SetFacePreset(PlanarReflectionFacePreset::PositiveY);
		ImGui::SameLine();
		if (ImGui::Button("-Y##PlanarReflectionFace")) SetFacePreset(PlanarReflectionFacePreset::NegativeY);
		ImGui::SameLine();
		if (ImGui::Button("+Z##PlanarReflectionFace")) SetFacePreset(PlanarReflectionFacePreset::PositiveZ);
		ImGui::SameLine();
		if (ImGui::Button("-Z##PlanarReflectionFace")) SetFacePreset(PlanarReflectionFacePreset::NegativeZ);

		changed |= ImGui::Checkbox("法線を反転##PlanarReflectionFlipNormal", &flipNormal_);
		changed |= ImGui::Checkbox("Receiver表面へ自動Fit##PlanarReflectionAutoFit", &autoFitToReceiverSurface_);
		changed |= ImGui::DragFloat("鏡面オフセット##PlanarReflectionPlaneOffset", &planeOffset_, 0.01f, -100.0f, 100.0f, "%.3f");
		changed |= ImGui::DragFloat("面判定許容幅##PlanarReflectionSurfaceTolerance", &surfaceTolerance_, 0.001f, 0.001f, 1.0f, "%.3f");
		changed |= ImGui::DragFloat("クリップバイアス##PlanarReflectionClipBias", &clipPlaneBias_, 0.001f, 0.0f, 1.0f, "%.3f");
		changed |= ImGui::Checkbox("平面を表示##PlanarReflectionDebugPlane", &debugPlaneVisible_);
		changed |= ImGui::DragFloat("デバッグ平面サイズ##PlanarReflectionDebugSize", &debugPlaneSize_, 0.1f, 0.1f, 100.0f);
		strength_ = std::clamp(strength_, 0.0f, 1.0f);
		surfaceTolerance_ = std::clamp(surfaceTolerance_, 0.001f, 1.0f);
		clipPlaneBias_ = std::clamp(clipPlaneBias_, 0.0f, 1.0f);
		debugPlaneSize_ = (std::max)(debugPlaneSize_, 0.1f);

		if (changed)
		{
			SyncToManager();
		}
		if (ImGui::Button("再キャプチャ##PlanarReflectionRecapture", ImVec2(-1.0f, 0.0f)))
		{
			RequestCapture();
		}

		PlanarReflectionManager* planarManager = PlanarReflectionManager::GetInstance();
		const PlanarReflectionDiagnostics diagnostics = planarManager->GetDiagnostics(this);
		const PlanarReflectionCaptureStats captureStats = PlanarReflectionCaptureDiagnostics::GetInstance()->Get(GetOwner());
		const PlanarReflectionBinding previewBinding = planarManager->ResolveBinding(this);
		const Vector3 planePosition = GetPlanePosition();
		ImGui::Text("状態: %s", diagnostics.captured ? (diagnostics.dirty ? "再Capture待ち" : "Captured") : "未Capture");
		ImGui::Text("Capture Revision: %llu", static_cast<unsigned long long>(diagnostics.captureRevision));
		ImGui::Text("Capture Resolution: %u x %u", diagnostics.captureWidth, diagnostics.captureHeight);
		ImGui::Text("Capture候補: %u (Opaque %u / Masked %u / Transparent %u / Additive %u)",
			captureStats.drawableCount,
			captureStats.opaqueCount,
			captureStats.maskedCount,
			captureStats.transparentCount,
			captureStats.additiveCount);
		ImGui::Text("Oblique Clip: %s", diagnostics.obliqueClipApplied ? "ON" : "OFF");
		ImGui::Text("鏡面位置: %.3f, %.3f, %.3f", planePosition.x, planePosition.y, planePosition.z);

		ImGui::SeparatorText("Capture RT Preview");
		if (previewBinding.valid && previewBinding.texture.ptr != 0 && diagnostics.captureWidth > 0 && diagnostics.captureHeight > 0)
		{
			const float availableWidth = (std::max)(ImGui::GetContentRegionAvail().x, 120.0f);
			const float previewWidth = (std::min)(availableWidth, 360.0f);
			const float aspect = static_cast<float>(diagnostics.captureWidth) / static_cast<float>(diagnostics.captureHeight);
			const float previewHeight = previewWidth / (std::max)(aspect, 0.001f);
			ImGui::Image(static_cast<ImTextureID>(previewBinding.texture.ptr), ImVec2(previewWidth, previewHeight)); // 鏡へ渡している実RTを表示し、CaptureとSurface Sampleのどちらが壊れているか直接確認する。
		}
		else
		{
			ImGui::TextDisabled("Capture済みReflection Textureはまだありません。");
		}

		ImGui::TextDisabled("Auto Fit ONでは同じActorのModel頂点から法線方向の最外面を鏡面にします。");
		ImGui::TextDisabled("同じActorへ最大6面分追加でき、各Componentが1枚の独立した鏡面になります。");
		ImGui::TextDisabled("Captureは全Component合計で1フレーム最大1面なので、複数面でも描画負荷を急増させません。");
		ImGui::TextDisabled("クリップバイアスは鏡面より裏側や接触面の映り込みをOblique Near Planeで除去します。");
		ImGui::TextDisabled("Local +Yが鏡面法線です。面判定許容幅の外側には鏡像を貼りません。");
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
		outJson["Quality"] = PlanarReflectionComponentDetail::QualityToString(quality_);
		outJson["FlipNormal"] = flipNormal_;
		outJson["AutoFitToReceiverSurface"] = autoFitToReceiverSurface_;
		outJson["PlaneOffset"] = planeOffset_;
		outJson["SurfaceTolerance"] = surfaceTolerance_;
		outJson["ClipPlaneBias"] = clipPlaneBias_;
		outJson["DebugPlaneVisible"] = debugPlaneVisible_;
		outJson["DebugPlaneSize"] = debugPlaneSize_;
	}

	inline void PlanarReflectionComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		if (const auto it = inJson.find("Enabled"); it != inJson.end() && it->is_boolean()) enabled_ = it->get<bool>();
		if (const auto it = inJson.find("Strength"); it != inJson.end() && it->is_number()) strength_ = it->get<float>();
		if (const auto it = inJson.find("UpdateMode"); it != inJson.end() && it->is_string()) updateMode_ = PlanarReflectionComponentDetail::UpdateModeFromString(it->get<std::string>());
		if (const auto it = inJson.find("Quality"); it != inJson.end() && it->is_string()) quality_ = PlanarReflectionComponentDetail::QualityFromString(it->get<std::string>());
		if (const auto it = inJson.find("FlipNormal"); it != inJson.end() && it->is_boolean()) flipNormal_ = it->get<bool>();
		if (const auto it = inJson.find("AutoFitToReceiverSurface"); it != inJson.end() && it->is_boolean()) autoFitToReceiverSurface_ = it->get<bool>();
		if (const auto it = inJson.find("PlaneOffset"); it != inJson.end() && it->is_number()) planeOffset_ = it->get<float>();
		if (const auto it = inJson.find("SurfaceTolerance"); it != inJson.end() && it->is_number()) surfaceTolerance_ = it->get<float>();
		if (const auto it = inJson.find("ClipPlaneBias"); it != inJson.end() && it->is_number()) clipPlaneBias_ = it->get<float>();
		if (const auto it = inJson.find("DebugPlaneVisible"); it != inJson.end() && it->is_boolean()) debugPlaneVisible_ = it->get<bool>();
		if (const auto it = inJson.find("DebugPlaneSize"); it != inJson.end() && it->is_number()) debugPlaneSize_ = it->get<float>();
		strength_ = std::clamp(strength_, 0.0f, 1.0f);
		surfaceTolerance_ = std::clamp(surfaceTolerance_, 0.001f, 1.0f);
		clipPlaneBias_ = std::clamp(clipPlaneBias_, 0.0f, 1.0f);
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

	inline void PlanarReflectionComponent::SetSurfaceTolerance(float tolerance)
	{
		surfaceTolerance_ = std::clamp(tolerance, 0.001f, 1.0f);
	}

	inline void PlanarReflectionComponent::SetClipPlaneBias(float bias)
	{
		clipPlaneBias_ = std::clamp(bias, 0.0f, 1.0f);
	}

	inline void PlanarReflectionComponent::SetFacePreset(PlanarReflectionFacePreset preset)
	{
		using namespace PlanarReflectionComponentDetail;
		Vector3 localRotation{};
		switch (preset)
		{
		case PlanarReflectionFacePreset::PositiveX:
			localRotation = { 0.0f, 0.0f, -kHalfPi };
			break;
		case PlanarReflectionFacePreset::NegativeX:
			localRotation = { 0.0f, 0.0f, kHalfPi };
			break;
		case PlanarReflectionFacePreset::NegativeY:
			localRotation = { kPi, 0.0f, 0.0f };
			break;
		case PlanarReflectionFacePreset::PositiveZ:
			localRotation = { kHalfPi, 0.0f, 0.0f };
			break;
		case PlanarReflectionFacePreset::NegativeZ:
			localRotation = { -kHalfPi, 0.0f, 0.0f };
			break;
		case PlanarReflectionFacePreset::PositiveY:
		default:
			localRotation = { 0.0f, 0.0f, 0.0f };
			break;
		}
		flipNormal_ = false;
		SetLocalRotation(localRotation);
		RefreshWorldTransform();
		SyncToManager(true); // 面プリセット変更時は対応するReflection Cameraを即座に再Capture対象へ戻す。
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

	inline Vector3 PlanarReflectionComponent::GetPlanePosition() const
	{
		const Vector3 normal = GetPlaneNormal();
		Vector3 planePosition = GetWorldPosition();
		if (autoFitToReceiverSurface_)
		{
			if (const Actor* owner = GetOwner())
			{
				const std::vector<const ModelComponent*> models = owner->GetComponents<ModelComponent>();
				float bestProjection = std::numeric_limits<float>::lowest();
				bool found = false;
				for (const ModelComponent* model : models)
				{
					if (!model || !model->IsActiveInHierarchy()) continue;
					Vector3 supportPoint{};
					if (!model->TryGetReflectionReceiverSurfacePoint(normal, supportPoint)) continue;
					const float projection = Vector3::Dot(supportPoint, normal);
					if (!found || projection > bestProjection)
					{
						bestProjection = projection;
						planePosition = supportPoint;
						found = true;
					}
				}
			}
		}
		return planePosition + normal * planeOffset_; // OffsetはAuto Fit後の面から法線方向へ微調整する。
	}

	inline PlanarReflectionDesc PlanarReflectionComponent::BuildDesc() const
	{
		PlanarReflectionDesc desc{};
		desc.position = GetPlanePosition();
		desc.normal = GetPlaneNormal();
		desc.strength = strength_;
		desc.surfaceTolerance = surfaceTolerance_;
		desc.clipPlaneBias = clipPlaneBias_;
		desc.updateMode = updateMode_;
		desc.quality = quality_;
		desc.enabled = enabled_ && IsActiveInHierarchy();
		return desc;
	}
} // namespace Ken4lowEngine
