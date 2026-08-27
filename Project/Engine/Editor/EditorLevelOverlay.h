#pragma once

#include "EditorContext.h"
#include "EditorDiagnosticsPanel.h"
#include "EditorLevelDeferredController.h"
#include "EditorLevelService.h"
#include "EditorPlayController.h"
#include "EditorPlaySessionManager.h"
#include "EditorProfilerPanel.h"
#include "EditorSceneDeferredController.h"
#include "EditorWindowManager.h"
#include "GpuFluidDiagnosticsPanel.h"
#include "GpuSphAdvancedDiagnosticsPanel.h"
#include "GpuSphRigidbodyInteractionDiagnosticsPanel.h"
#include "GpuVolumetricFluidDiagnosticsPanel.h"

#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
#ifdef USE_IMGUI
	/// <summary>レベル操作と現在のエディタ / 実行中ワールドの状態をメインビューポート右上へ表示します。</summary>
	inline void DrawEditorLevelOverlay()
	{
		EditorWindowManager* windowManager = EditorWindowManager::GetInstance();
		EditorDiagnosticsPanel::GetInstance()->Draw();
		EditorProfilerPanel::GetInstance()->Draw(windowManager->GetSceneManager(), &windowManager->GetPerformanceMonitor());
		GpuFluidDiagnosticsPanel::GetInstance()->Draw(); // GPU流体とSPHの診断画面をエディタ更新から描画する。
		GpuSphAdvancedDiagnosticsPanel::GetInstance()->Draw();
		GpuSphRigidbodyInteractionDiagnosticsPanel::GetInstance()->Draw();
		GpuVolumetricFluidDiagnosticsPanel::GetInstance()->Draw();

		EditorLevelService* levelService = EditorLevelService::GetInstance();
		EditorLevelDeferredController* deferredController = EditorLevelDeferredController::GetInstance();
		EditorSceneDeferredController* sceneController = EditorSceneDeferredController::GetInstance();
		EditorPlayController* playController = EditorPlayController::GetInstance();
		EditorPlaySessionManager* sessionManager = EditorPlaySessionManager::GetInstance();
		levelService->SetSceneManager(windowManager->GetSceneManager());
		sceneController->SetSceneManager(windowManager->GetSceneManager());
		levelService->Update(ImGui::GetIO().DeltaTime);
		deferredController->UpdateShortcuts();
		sceneController->Update(); // シーン切り替えは描画中に実行せず、更新処理へ遅延させる。

		const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
		if (viewportRect.valid)
		{
			const float width = sessionManager->IsSessionActive() ? 470.0f : 310.0f;
			ImGui::SetNextWindowPos(
				ImVec2(viewportRect.screenMax.x - width - 8.0f, viewportRect.screenMin.y + 54.0f),
				ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(width, 42.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.94f);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_NoDocking |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoScrollWithMouse |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav;

			if (ImGui::Begin("##EditorLevelToolbar", nullptr, flags))
			{
				const bool levelEditable = playController->IsEditing() && !playController->IsTransitionPending();
				if (!levelEditable) ImGui::BeginDisabled();
				if (ImGui::Button("シーン", ImVec2(58.0f, 24.0f))) ImGui::OpenPopup("##EditorSceneMenu");
				if (!levelEditable) ImGui::EndDisabled();
				if (ImGui::BeginPopup("##EditorSceneMenu"))
				{
					sceneController->DrawMenuItems();
					ImGui::EndPopup();
				}

				ImGui::SameLine();
				if (!levelEditable) ImGui::BeginDisabled();
				if (ImGui::Button("レベル", ImVec2(58.0f, 24.0f))) ImGui::OpenPopup("##EditorLevelMenu");
				if (!levelEditable) ImGui::EndDisabled();

				if (ImGui::BeginPopup("##EditorLevelMenu"))
				{
					deferredController->DrawFileMenuItems();
					ImGui::EndPopup();
				}

				ImGui::SameLine();
				if (!levelEditable) ImGui::BeginDisabled();
				if (ImGui::Button("保存", ImVec2(48.0f, 24.0f))) levelService->RequestSaveLevel();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip(levelEditable ? "レベルを保存  Ctrl+S" : "実行中はエディタ側のレベルを保存できません。");
				}
				if (!levelEditable) ImGui::EndDisabled();

				ImGui::SameLine();
				const EditorContext* context = EditorContext::GetInstance();
				const std::string displayName = context->GetActiveLevelName() + (context->IsLevelDirty() ? "*" : "");
				ImGui::TextDisabled("%s", displayName.c_str());

				if (sessionManager->IsSessionActive())
				{
					ImGui::SameLine();
					if (ImGui::Button("実行状態", ImVec2(92.0f, 24.0f))) ImGui::OpenPopup("##PIERuntimeStatus");
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("エディタとは別のActorインスタンスで実行中です。停止すると実行前の状態へ戻ります。");
					}
					if (ImGui::BeginPopup("##PIERuntimeStatus"))
					{
						ImGui::TextUnformatted("実行中ワールド");
						ImGui::Separator();
						ImGui::Text("経過時間: %.2f 秒", sessionManager->GetRuntimeElapsedSeconds());
						ImGui::Text("フレーム数: %llu", static_cast<unsigned long long>(sessionManager->GetRuntimeFrameCount()));
						ImGui::Text("Actor数: %zu", sessionManager->GetRuntimeActorCount());
						ImGui::Text("状態: %s", playController->GetPlayStateText());
						if (playController->IsPaused() && ImGui::MenuItem("1フレーム実行")) playController->Step();
						if (ImGui::MenuItem("実行中の変更を反映して停止")) playController->KeepChangesAndStop();
						ImGui::TextDisabled("通常の停止では実行中の変更を破棄します。");
						ImGui::EndPopup();
					}
				}
			}
			ImGui::End();
		}

		deferredController->DrawDialogs(); // 新規作成や読み込みは描画中にリソースを破棄せず、次の更新処理へ予約する。

		std::string statusMessage;
		bool succeeded = false;
		if (levelService->ConsumeStatus(statusMessage, succeeded))
		{
			windowManager->AddOutputLog(succeeded ? EditorLogLevel::Info : EditorLogLevel::Error, statusMessage);
		}
	}
#endif
} // namespace Ken4lowEngine
