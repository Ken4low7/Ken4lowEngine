#pragma once

#include "EditorContext.h"
#include "EditorDiagnosticsPanel.h"
#include "EditorLevelDeferredController.h"
#include "EditorLevelService.h"
#include "EditorPlayController.h"
#include "EditorPlaySessionManager.h"
#include "EditorWindowManager.h"

#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
#ifdef USE_IMGUI
	/// <summary>Level操作と現在のEditor / PIE World状態をMain Viewport右上へ表示します。</summary>
	inline void DrawEditorLevelOverlay()
	{
		EditorDiagnosticsPanel::GetInstance()->Draw(); // Phase 12のLog / Profiler / Error統合PanelをEditor中は毎フレーム更新する。

		EditorWindowManager* windowManager = EditorWindowManager::GetInstance();
		EditorLevelService* levelService = EditorLevelService::GetInstance();
		EditorLevelDeferredController* deferredController = EditorLevelDeferredController::GetInstance();
		EditorPlayController* playController = EditorPlayController::GetInstance();
		EditorPlaySessionManager* sessionManager = EditorPlaySessionManager::GetInstance();
		levelService->SetSceneManager(windowManager->GetSceneManager());
		levelService->Update(ImGui::GetIO().DeltaTime);
		deferredController->UpdateShortcuts();

		const EditorViewportRect& viewportRect = windowManager->GetMainViewportRect();
		if (viewportRect.valid)
		{
			const float width = sessionManager->IsSessionActive() ? 400.0f : 240.0f;
			ImGui::SetNextWindowPos(
				ImVec2(viewportRect.screenMax.x - width - 8.0f, viewportRect.screenMin.y + 54.0f),
				ImGuiCond_Always); // メインToolbarと別段へ配置し、背面へ隠れて操作不能になる重なりを避ける。
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
				if (ImGui::Button("Level", ImVec2(58.0f, 24.0f))) ImGui::OpenPopup("##EditorLevelMenu");
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
					ImGui::SetTooltip(levelEditable ? "Level保存  Ctrl+S" : "PIE中はEditor Levelを保存できません。");
				}
				if (!levelEditable) ImGui::EndDisabled();

				ImGui::SameLine();
				const EditorContext* context = EditorContext::GetInstance();
				const std::string displayName = context->GetActiveLevelName() + (context->IsLevelDirty() ? "*" : "");
				ImGui::TextDisabled("%s", displayName.c_str());

				if (sessionManager->IsSessionActive())
				{
					ImGui::SameLine();
					if (ImGui::Button("PIE Runtime", ImVec2(92.0f, 24.0f))) ImGui::OpenPopup("##PIERuntimeStatus");
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Editor Worldとは別Actorインスタンスで実行中です。StopでPlay前へ戻ります。");
					}
					if (ImGui::BeginPopup("##PIERuntimeStatus"))
					{
						ImGui::TextUnformatted("PIE Runtime World");
						ImGui::Separator();
						ImGui::Text("Time: %.2f sec", sessionManager->GetRuntimeElapsedSeconds());
						ImGui::Text("Frames: %llu", static_cast<unsigned long long>(sessionManager->GetRuntimeFrameCount()));
						ImGui::Text("Actors: %zu", sessionManager->GetRuntimeActorCount());
						ImGui::Text("State: %s", playController->GetPlayStateText());
						if (playController->IsPaused() && ImGui::MenuItem("1フレーム実行")) playController->Step();
						if (ImGui::MenuItem("Runtime変更を反映して停止")) playController->KeepChangesAndStop();
						ImGui::TextDisabled("通常のStopではRuntime変更を破棄します。");
						ImGui::EndPopup();
					}
				}
			}
			ImGui::End();
		}

		deferredController->DrawDialogs(); // New/Openは描画中にActorやGPU Resourceを破棄せず次のUpdateへ予約する。

		std::string statusMessage;
		bool succeeded = false;
		if (levelService->ConsumeStatus(statusMessage, succeeded))
		{
			windowManager->AddOutputLog(succeeded ? EditorLogLevel::Info : EditorLogLevel::Error, statusMessage);
		}
	}
#endif
} // namespace Ken4lowEngine
