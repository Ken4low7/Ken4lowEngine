#define NOMINMAX
#include "EditorTransformGizmo.h"

#ifdef USE_IMGUI
#include "EditorCommandHistory.h"
#include "EditorContext.h"
#include "EditorLevelOverlay.h"
#include "EditorModeController.h"
#include "EditorPlayController.h"
#include "EditorViewportController.h"
#include "EditorWindowManager.h"

#include <CameraManager.h>
#include <DebugCamera.h>
#include <Matrix4x4.h>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include <Externals/ImGuizmo/ImGuizmo.cpp> // ImGuizmo側でMath Operatorを定義してからImGui内部実装を読み込む。
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <memory>

namespace Ken4lowEngine
{
	namespace
	{
		ImGuizmo::OPERATION ToImGuizmoOperation(EditorViewportTool tool)
		{
			switch (tool)
			{
			case EditorViewportTool::Rotate: return ImGuizmo::ROTATE;
			case EditorViewportTool::Scale: return ImGuizmo::SCALE;
			case EditorViewportTool::Translate:
			default: return ImGuizmo::TRANSLATE;
			}
		}

		float KeepScaleInvertible(float value)
		{
			if (!std::isfinite(value)) return 1.0f;
			if (std::abs(value) >= 0.001f) return value;
			return value < 0.0f ? -0.001f : 0.001f;
		}

		bool IsSameVector(const Vector3& lhs, const Vector3& rhs)
		{
			constexpr float epsilon = 0.00001f;
			return std::abs(lhs.x - rhs.x) <= epsilon && std::abs(lhs.y - rhs.y) <= epsilon && std::abs(lhs.z - rhs.z) <= epsilon;
		}

		bool IsSameTransform(const EditorTransform& lhs, const EditorTransform& rhs)
		{
			return IsSameVector(lhs.position, rhs.position) && IsSameVector(lhs.rotation, rhs.rotation) && IsSameVector(lhs.scale, rhs.scale);
		}

		void UpdateToolShortcuts(EditorViewportController& controller)
		{
			const ImGuiIO& io = ImGui::GetIO();
			if (io.WantTextInput || ImGui::IsAnyItemActive() || io.MouseDown[ImGuiMouseButton_Right] || io.MouseDown[ImGuiMouseButton_Middle]) return;
			if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) controller.SetTool(EditorViewportTool::Select);
			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) controller.SetTool(EditorViewportTool::Translate);
			if (ImGui::IsKeyPressed(ImGuiKey_E, false)) controller.SetTool(EditorViewportTool::Rotate);
			if (ImGui::IsKeyPressed(ImGuiKey_R, false)) controller.SetTool(EditorViewportTool::Scale);
		}

		void FocusDebugCamera(const EditorObjectInfo& selected)
		{
			EditorTransform worldTransform{};
			CameraManager* cameraManager = CameraManager::GetInstance();
			DebugCamera* debugCamera = cameraManager->GetDebugCamera();
			if (!cameraManager->IsUsingDebugCamera() || !debugCamera || !selected.TryReadWorldTransform(worldTransform)) return;
			debugCamera->SetTranslate(worldTransform.position - cameraManager->GetActiveCameraForward() * 8.0f);
			debugCamera->RefreshViewProjection();
		}

		void ReleaseViewportImageItemForGizmo(ImGuiWindow* viewportWindow)
		{
			ImGuiContext& context = *ImGui::GetCurrentContext();
			if (context.HoveredWindow != viewportWindow || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
			context.HoveredId = 0;
			context.HoveredIdPreviousFrame = 0;
			ImGui::ClearActiveID(); // MainViewportのInvisibleButtonだけを解除してImGuizmoへ左クリックを渡す。
		}
	}

	EditorTransformGizmo* EditorTransformGizmo::GetInstance()
	{
		static EditorTransformGizmo instance;
		return &instance;
	}

	void EditorTransformGizmo::BeginTransformCommand(const EditorObjectInfo& target, const EditorTransform& before)
	{
		transformCommandActive_ = true;
		transformCommandTarget_ = target;
		transformCommandBefore_ = before; // ドラッグ開始時の値を1回だけ保存して連続フレームを1履歴へまとめる。
	}

	void EditorTransformGizmo::EndTransformCommand()
	{
		if (!transformCommandActive_) return;

		EditorTransform after{};
		if (transformCommandTarget_.TryReadWorldTransform(after) && !IsSameTransform(transformCommandBefore_, after))
		{
			const EditorObjectInfo target = transformCommandTarget_;
			EditorCommandHistory::GetInstance()->PushExecuted(std::make_unique<EditorValueCommand<EditorTransform>>(
				"Transform変更",
				transformCommandBefore_,
				after,
				[target](const EditorTransform& value)
				{
					target.WriteWorldTransform(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}
		transformCommandActive_ = false;
		transformCommandTarget_ = {};
	}

	void EditorTransformGizmo::Draw()
	{
		if (!EditorModeController::GetInstance()->IsEditorModeEnabled())
		{
			EndTransformCommand();
			return;
		}

		DrawEditorLevelOverlay(); // 選択状態に関係なくLevel保存・読込UIとAuto Saveを毎フレーム更新する。

		if (EditorPlayController::GetInstance()->IsPlaying())
		{
			EndTransformCommand();
			return;
		}

		EditorViewportController& viewportController = *EditorViewportController::GetInstance();
		if (!viewportController.IsEditorDisplay())
		{
			EndTransformCommand();
			return;
		}
		UpdateToolShortcuts(viewportController);

		EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
		if (!selection.HasSelection())
		{
			EndTransformCommand();
			return;
		}

		const EditorObjectInfo& selected = selection.GetSelected();
		if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()) FocusDebugCamera(selected);
		if (viewportController.GetTool() == EditorViewportTool::Select || !selected.canEditTransform)
		{
			EndTransformCommand();
			return;
		}

		const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
		if (!viewportRect.valid || viewportRect.imageSize.x <= 1.0f || viewportRect.imageSize.y <= 1.0f) return;

		ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Main Viewport");
		if (!viewportWindow) return;

		EditorTransform worldTransform{};
		if (!selected.TryReadWorldTransform(worldTransform)) return;

		Matrix4x4 transformMatrix = Matrix4x4::MakeAffineMatrix(worldTransform.scale, worldTransform.rotation, worldTransform.position);
		const Matrix4x4 view = CameraManager::GetInstance()->GetActiveViewMatrix();
		const Matrix4x4 projection = CameraManager::GetInstance()->GetActiveProjectionMatrix();

		ImGuizmo::BeginFrame();
		ImGuizmo::Enable(true);
		ImGuizmo::AllowAxisFlip(false);
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetGizmoSizeClipSpace(0.16f);
		ImGuizmo::Style& gizmoStyle = ImGuizmo::GetStyle();
		gizmoStyle.TranslationLineThickness = 4.0f;
		gizmoStyle.TranslationLineArrowSize = 8.0f;
		gizmoStyle.RotationLineThickness = 5.0f;
		gizmoStyle.RotationOuterLineThickness = 4.0f; // Yを含む回転Ringの選択幅を広げて軸クリックを安定させる。
		gizmoStyle.ScaleLineThickness = 4.0f;
		gizmoStyle.ScaleLineCircleSize = 7.0f;
		gizmoStyle.CenterCircleSize = 6.0f;
		ImGuizmo::SetDrawlist(viewportWindow->DrawList);
		ImGuizmo::SetAlternativeWindow(viewportWindow);
		ImGuizmo::SetRect(viewportRect.screenMin.x, viewportRect.screenMin.y, viewportRect.imageSize.x, viewportRect.imageSize.y);
		ReleaseViewportImageItemForGizmo(viewportWindow);

		const EditorViewportTool activeTool = viewportController.GetTool();
		const ImGuizmo::OPERATION operation = ToImGuizmoOperation(activeTool);
		const ImGuizmo::MODE mode = activeTool == EditorViewportTool::Scale || viewportController.GetGizmoSpace() == EditorGizmoSpace::Local
			? ImGuizmo::LOCAL : ImGuizmo::WORLD;

		float snap[3] = {};
		const float* snapValues = nullptr;
		if (viewportController.IsSnapEnabled())
		{
			if (activeTool == EditorViewportTool::Translate)
			{
				const Vector3& translationSnap = viewportController.GetTranslationSnap();
				snap[0] = std::max(0.001f, translationSnap.x);
				snap[1] = std::max(0.001f, translationSnap.y);
				snap[2] = std::max(0.001f, translationSnap.z);
			}
			else if (activeTool == EditorViewportTool::Rotate) snap[0] = std::max(0.1f, viewportController.GetRotationSnapDegrees());
			else
			{
				snap[0] = std::max(0.001f, viewportController.GetScaleSnap());
				snap[1] = snap[0];
				snap[2] = snap[0];
			}
			snapValues = snap;
		}

		const bool changed = ImGuizmo::Manipulate(
			&view.m[0][0], &projection.m[0][0], operation, mode,
			&transformMatrix.m[0][0], nullptr, snapValues);
		const bool usingGizmo = ImGuizmo::IsUsing();
		if (usingGizmo && !transformCommandActive_) BeginTransformCommand(selected, worldTransform);

		if (changed)
		{
			Vector3 scale{};
			Vector3 rotation{};
			Vector3 translation{};
			Matrix4x4::Decompose(transformMatrix, scale, rotation, translation); // EngineのXYZ回転順で分解し、Y軸回転をImGuizmo固有Euler変換から切り離す。

			EditorTransform editedWorldTransform{};
			editedWorldTransform.position = translation;
			editedWorldTransform.rotation = rotation;
			editedWorldTransform.scale = {
				KeepScaleInvertible(scale.x),
				KeepScaleInvertible(scale.y),
				KeepScaleInvertible(scale.z),
			};
			selected.WriteWorldTransform(editedWorldTransform);
			EditorContext::GetInstance()->MarkLevelDirty();
		}

		if (transformCommandActive_ && !usingGizmo) EndTransformCommand();
	}

	bool EditorTransformGizmo::IsUsing() const { return ImGuizmo::IsUsing(); }

	bool EditorTransformGizmo::IsOver() const
	{
		const EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
		return selection.HasSelection() && EditorViewportController::GetInstance()->GetTool() != EditorViewportTool::Select && ImGuizmo::IsOver();
	}
} // namespace Ken4lowEngine
#endif // USE_IMGUI
