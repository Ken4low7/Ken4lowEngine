#include "Actor.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Engine/Graphics/Renderer/Reflection/PlanarReflectionCaptureDiagnostics.h"
#include "Matrix4x4.h"
#include "ModelComponent.h"
#include "Plane.h"
#include "Wireframe.h"

#include <algorithm>
#include <array>
#include <cmath>
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
		inline constexpr float kAutoNormalFlatnessThreshold = 1.25f;

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

		inline Matrix4x4 BuildExactHierarchyRotation(const SceneComponent* component)
		{
			Matrix4x4 rotation = Matrix4x4::MakeIdentity();
			for (const SceneComponent* current = component; current; current = current->GetParent())
			{
				rotation = Matrix4x4::Multiply(rotation, Matrix4x4::MakeRotateMatrix(current->GetLocalRotation()));
			}
			return rotation; // Euler角の単純加算ではなく子→親の行列積で面法線をWorldへ変換する。
		}
	}

	inline void PlanarReflectionComponent::Initialize()
	{
		SceneComponent::Initialize();
		PlanarReflectionManager::GetInstance()->Initialize(DirectXCommon::GetInstance());
		InvalidateAutoNormalCache();
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
		// 鏡面反射の設定とキャプチャ診断を日本語で確認できるようにする。
		ImGui::SeparatorText("平面反射（鏡）");
		bool changed = false;
		changed |= ImGui::Checkbox("有効##PlanarReflectionEnabled", &enabled_);
		changed |= ImGui::SliderFloat("反射強度##PlanarReflectionStrength", &strength_, 0.0f, 1.0f, "%.2f");

		const char* updateModeNames[] = { "必要時のみ", "毎フレーム" };
		int updateModeIndex = updateMode_ == PlanarReflectionUpdateMode::OnDemand ? 0 : 1;
		if (ImGui::Combo("更新モード##PlanarReflectionUpdateMode", &updateModeIndex, updateModeNames, IM_ARRAYSIZE(updateModeNames)))
		{
			updateMode_ = updateModeIndex == 0 ? PlanarReflectionUpdateMode::OnDemand : PlanarReflectionUpdateMode::EveryFrame;
			changed = true;
		}

		const char* qualityNames[] = { "低 (25%)", "中 (50%)", "高 (75%)", "最高 (100%)" };
		int qualityIndex = static_cast<int>(quality_);
		if (ImGui::Combo("反射品質##PlanarReflectionQuality", &qualityIndex, qualityNames, IM_ARRAYSIZE(qualityNames)))
		{
			quality_ = static_cast<PlanarReflectionQuality>(qualityIndex);
			changed = true;
		}

		if (ImGui::Checkbox("反射対象の形状から面方向を自動判定##PlanarReflectionAutoNormal", &autoDetectReceiverNormal_))
		{
			InvalidateAutoNormalCache();
			changed = true;
		}
		ImGui::TextDisabled(autoDetectReceiverNormal_
			? "平たい反射対象は最薄軸、立方体などはローカルZ軸を前後軸として使います。"
			: "手動面では反射対象のローカル軸を指定し、親の変換を含めてワールド法線へ変換します。");

		ImGui::TextDisabled("手動面プリセット（押すと自動判定を無効にします）");
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
		changed |= ImGui::Checkbox("反射対象の表面へ自動フィット##PlanarReflectionAutoFit", &autoFitToReceiverSurface_);
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
		const Vector3 planeNormal = GetPlaneNormal();
		ImGui::Text("状態: %s", diagnostics.captured ? (diagnostics.dirty ? "再キャプチャ待ち" : "キャプチャ済み") : "未キャプチャ");
		ImGui::Text("キャプチャ更新番号: %llu", static_cast<unsigned long long>(diagnostics.captureRevision));
		ImGui::Text("キャプチャ解像度: %u x %u", diagnostics.captureWidth, diagnostics.captureHeight);
		ImGui::Text("キャプチャ候補: %u (不透明 %u / マスク %u / 半透明 %u / 加算 %u)",
			captureStats.drawableCount,
			captureStats.opaqueCount,
			captureStats.maskedCount,
			captureStats.transparentCount,
			captureStats.additiveCount);
		ImGui::Text("斜めクリップ: %s", diagnostics.obliqueClipApplied ? "有効" : "無効");
		ImGui::Text("面方向: %s", autoDetectReceiverNormal_ ? "自動 / 反射対象の形状" : "手動 / 反射対象のローカル軸");
		ImGui::Text("鏡面法線: %.3f, %.3f, %.3f", planeNormal.x, planeNormal.y, planeNormal.z);
		ImGui::Text("鏡面位置: %.3f, %.3f, %.3f", planePosition.x, planePosition.y, planePosition.z);

		ImGui::SeparatorText("キャプチャ画像プレビュー");
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
			ImGui::TextDisabled("キャプチャ済みの反射テクスチャはまだありません。");
		}

		ImGui::TextDisabled("自動法線は平たい反射対象の最薄軸を優先し、立方体などではローカルZ軸を前後軸として固定します。");
		ImGui::TextDisabled("自動フィット有効時は同じActorのモデル頂点から法線方向の最外面を鏡面にします。");
		ImGui::TextDisabled("同じActorへ最大6面分追加でき、各Componentが1枚の独立した鏡面になります。");
		ImGui::TextDisabled("キャプチャは全Component合計で1フレーム最大1面なので、複数面でも描画負荷を急増させません。");
		ImGui::TextDisabled("クリップバイアスは鏡面より裏側や接触面の映り込みを斜めNear Planeで除去します。");
		if (updateMode_ == PlanarReflectionUpdateMode::OnDemand)
		{
			ImGui::TextDisabled("必要時のみ更新では、カメラ移動後に再キャプチャが必要です。");
		}
		else
		{
			ImGui::TextDisabled("毎フレーム更新は正確な鏡用ですが、シーンを追加描画するため高負荷です。");
		}
#endif
	}

	inline void PlanarReflectionComponent::Finalize()
	{
		InvalidateAutoNormalCache();
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
		outJson["AutoDetectReceiverNormal"] = autoDetectReceiverNormal_;
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
		if (const auto it = inJson.find("AutoDetectReceiverNormal"); it != inJson.end() && it->is_boolean()) autoDetectReceiverNormal_ = it->get<bool>();
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
		InvalidateAutoNormalCache();
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
		autoDetectReceiverNormal_ = false;
		InvalidateAutoNormalCache();
		flipNormal_ = false;
		SetLocalRotation(localRotation);
		RefreshWorldTransform();
		SyncToManager(true); // 手動面はAuto Normalを解除し、指定したReceiver Local面を即Capture対象へ戻す。
	}

	inline bool PlanarReflectionComponent::TryResolveAutoPlaneNormal(Vector3& outNormal) const
	{
		if (!autoDetectReceiverNormal_) return false;
		const Actor* owner = GetOwner();
		if (!owner) return false;

		const std::vector<const PlanarReflectionComponent*> planarComponents = owner->GetComponents<PlanarReflectionComponent>();
		std::size_t activePlanarCount = 0;
		for (const PlanarReflectionComponent* planar : planarComponents)
		{
			if (planar && planar->IsActiveInHierarchy() && planar->IsEnabled()) ++activePlanarCount;
		}
		if (activePlanarCount > 1) return false; // 6面鏡など複数面Actorは各Componentの手動面を保持する。

		const std::vector<const ModelComponent*> models = owner->GetComponents<ModelComponent>();
		if (autoNormalCacheValid_ && autoNormalReceiver_)
		{
			const auto cachedIt = std::find(models.begin(), models.end(), autoNormalReceiver_);
			if (cachedIt != models.end() && autoNormalReceiver_->IsActiveInHierarchy() &&
				autoNormalReceiver_->GetWorldTransformRevision() == autoNormalReceiverRevision_)
			{
				Vector3 resolvedAxis = autoNormalAxis_;
				if (Vector3::Dot(CameraManager::GetInstance()->GetActiveCameraPosition(), resolvedAxis) < autoNormalCenterProjection_)
				{
					resolvedAxis = -resolvedAxis;
				}
				outNormal = Vector3::NormalizeSafe(resolvedAxis, { 0.0f, 1.0f, 0.0f });
				return true;
			}
		}

		float bestFlatness = 0.0f;
		const ModelComponent* bestReceiver = nullptr;
		Vector3 bestAxis{ 0.0f, 1.0f, 0.0f };
		float bestCenterProjection = 0.0f;
		const ModelComponent* fallbackReceiver = nullptr;
		Vector3 fallbackAxis{ 0.0f, 0.0f, 1.0f };
		float fallbackCenterProjection = 0.0f;

		for (const ModelComponent* model : models)
		{
			if (!model || !model->IsActiveInHierarchy()) continue;

			const Matrix4x4 receiverRotation = Matrix4x4::MakeRotateMatrix(model->GetWorldRotation());
			const std::array<Vector3, 3> axes = {
				Vector3::NormalizeSafe(Vector3::Transform({ 1.0f, 0.0f, 0.0f }, receiverRotation), { 1.0f, 0.0f, 0.0f }),
				Vector3::NormalizeSafe(Vector3::Transform({ 0.0f, 1.0f, 0.0f }, receiverRotation), { 0.0f, 1.0f, 0.0f }),
				Vector3::NormalizeSafe(Vector3::Transform({ 0.0f, 0.0f, 1.0f }, receiverRotation), { 0.0f, 0.0f, 1.0f })
			};

			std::array<float, 3> thickness{};
			std::array<float, 3> centerProjection{};
			bool validModel = true;
			for (std::size_t axisIndex = 0; axisIndex < axes.size(); ++axisIndex)
			{
				Vector3 positiveSupport{};
				Vector3 negativeSupport{};
				if (!model->TryGetReflectionReceiverSurfacePoint(axes[axisIndex], positiveSupport) ||
					!model->TryGetReflectionReceiverSurfacePoint(-axes[axisIndex], negativeSupport))
				{
					validModel = false;
					break;
				}

				const float positiveProjection = Vector3::Dot(positiveSupport, axes[axisIndex]);
				const float negativeProjection = Vector3::Dot(negativeSupport, axes[axisIndex]);
				const float minimumProjection = (std::min)(positiveProjection, negativeProjection);
				const float maximumProjection = (std::max)(positiveProjection, negativeProjection);
				thickness[axisIndex] = maximumProjection - minimumProjection;
				centerProjection[axisIndex] = (minimumProjection + maximumProjection) * 0.5f;
			}
			if (!validModel) continue;

			if (!fallbackReceiver)
			{
				fallbackReceiver = model;
				fallbackAxis = axes[2];
				fallbackCenterProjection = centerProjection[2]; // 形状だけで面を決められないReceiverはLocal Zを固定の前後軸として使う。
			}

			std::size_t thinnestIndex = 0;
			for (std::size_t axisIndex = 1; axisIndex < thickness.size(); ++axisIndex)
			{
				if (thickness[axisIndex] < thickness[thinnestIndex]) thinnestIndex = axisIndex;
			}

			float secondSmallest = std::numeric_limits<float>::max();
			for (std::size_t axisIndex = 0; axisIndex < thickness.size(); ++axisIndex)
			{
				if (axisIndex == thinnestIndex) continue;
				secondSmallest = (std::min)(secondSmallest, thickness[axisIndex]);
			}

			const float flatness = secondSmallest / (std::max)(thickness[thinnestIndex], 0.0001f);
			if (flatness >= PlanarReflectionComponentDetail::kAutoNormalFlatnessThreshold && flatness > bestFlatness)
			{
				bestFlatness = flatness;
				bestReceiver = model;
				bestAxis = axes[thinnestIndex];
				bestCenterProjection = centerProjection[thinnestIndex];
			}
		}

		if (!bestReceiver && fallbackReceiver)
		{
			bestReceiver = fallbackReceiver;
			bestAxis = fallbackAxis;
			bestCenterProjection = fallbackCenterProjection; // Cameraを回しても鏡面の前後軸がX/Z間で切り替わらないよう固定する。
		}
		if (!bestReceiver)
		{
			InvalidateAutoNormalCache();
			return false;
		}

		autoNormalCacheValid_ = true;
		autoNormalReceiver_ = bestReceiver;
		autoNormalReceiverRevision_ = bestReceiver->GetWorldTransformRevision();
		autoNormalAxis_ = Vector3::NormalizeSafe(bestAxis, { 0.0f, 1.0f, 0.0f });
		autoNormalCenterProjection_ = bestCenterProjection;

		Vector3 resolvedAxis = autoNormalAxis_;
		if (Vector3::Dot(CameraManager::GetInstance()->GetActiveCameraPosition(), resolvedAxis) < autoNormalCenterProjection_)
		{
			resolvedAxis = -resolvedAxis;
		}
		outNormal = resolvedAxis;
		return true;
	}

	inline Vector3 PlanarReflectionComponent::GetPlaneNormal() const
	{
		Vector3 normal{};
		if (!TryResolveAutoPlaneNormal(normal))
		{
			const Matrix4x4 rotation = PlanarReflectionComponentDetail::BuildExactHierarchyRotation(this);
			normal = Vector3::NormalizeSafe(
				Vector3::Transform({ 0.0f, 1.0f, 0.0f }, rotation),
				{ 0.0f, 1.0f, 0.0f });
		}
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