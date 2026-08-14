#pragma once

#include "EditorContext.h"
#include "EditorLevelDeferredController.h"
#include "EditorLevelService.h"
#include "EditorPlayController.h"

#include <SceneManager.h>

#include <filesystem>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>New Level / Save Asを連結し、保存したLevelをそのままDataDriven Sceneとして開きます。</summary>
	class EditorSceneDeferredController
	{
	public:
		static EditorSceneDeferredController* GetInstance()
		{
			static EditorSceneDeferredController instance;
			return &instance;
		}

		void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

#ifdef USE_IMGUI
		void Update()
		{
			if (state_ == State::WaitingForEmptyLevel)
			{
				EditorLevelService* levelService = EditorLevelService::GetInstance();
				if (levelService->GetCurrentLevelPath().empty() &&
					EditorContext::GetInstance()->GetActiveLevelName() == "Untitled")
				{
					levelService->RequestSaveLevelAs();
					state_ = State::WaitingForSave;
					savePopupObserved_ = false; // Save As完了後にLevel名をScene IDとして開く。
				}
			}
			else if (state_ == State::WaitingForSave)
			{
				EditorLevelService* levelService = EditorLevelService::GetInstance();
				const bool popupOpen = ImGui::IsPopupOpen("Save Level As##EditorLevelSaveAs");
				if (popupOpen) savePopupObserved_ = true;

				const std::filesystem::path& levelPath = levelService->GetCurrentLevelPath();
				if (!levelPath.empty())
				{
					const std::string sceneId = levelPath.stem().string();
					state_ = State::Idle;
					savePopupObserved_ = false;
					if (sceneManager_) sceneManager_->ChangeScene(sceneId); // Level自動検出を通し、作成Sceneへ即座に移動する。
				}
				else if (savePopupObserved_ && !popupOpen)
				{
					state_ = State::Idle; // Save Asをキャンセルした場合はNew Scene処理も終了する。
					savePopupObserved_ = false;
				}
			}
		}

		void DrawMenuItems()
		{
			const bool canCreate = EditorPlayController::GetInstance()->IsEditing() && state_ == State::Idle;
			const SceneDefinition* currentDefinition = sceneManager_ ? &sceneManager_->GetCurrentSceneDefinition() : nullptr;
			const std::string currentSceneId = currentDefinition && !currentDefinition->id.empty()
				? currentDefinition->id
				: std::string("(none)");

			ImGui::Text("Current: %s", currentSceneId.c_str());
			if (currentDefinition && !currentDefinition->levelPath.empty())
			{
				ImGui::TextDisabled("%s", currentDefinition->levelPath.c_str());
			}
			ImGui::Separator();

			if (ImGui::MenuItem("New Scene...", nullptr, false, canCreate)) RequestNewScene();

			const std::vector<std::string> sceneIds = sceneManager_
				? sceneManager_->GetAvailableSceneIds()
				: std::vector<std::string>{};
			if (ImGui::BeginMenu("Open Scene", !sceneIds.empty()))
			{
				for (const std::string& sceneId : sceneIds)
				{
					const bool isCurrent = sceneId == currentSceneId;
					const SceneDefinition* definition = sceneManager_->FindSceneDefinition(sceneId);
					const bool dataScene = definition && definition->className == "DataDrivenScene" && !definition->levelPath.empty();
					const std::string label = sceneId + (dataScene ? "  [Data]" : "  [C++]") + "##SceneMenu" + sceneId;
					if (ImGui::MenuItem(label.c_str(), nullptr, isCurrent, !isCurrent))
					{
						sceneManager_->ChangeScene(sceneId); // Scene切替はGpuSafeSceneTransitionを通して次の安全な境界で適用する。
					}
					if (ImGui::IsItemHovered() && definition && !definition->levelPath.empty())
					{
						ImGui::SetTooltip("%s", definition->levelPath.c_str());
					}
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();
			ImGui::TextDisabled("Data Scenes: Resources/JSON/Levels/*.json");
			ImGui::TextDisabled("Create -> Save -> appears automatically in Scene UI");
		}
#endif

		void RequestNewScene()
		{
			if (!EditorPlayController::GetInstance()->IsEditing() || state_ != State::Idle) return;
			state_ = State::WaitingForEmptyLevel;
			EditorLevelDeferredController::GetInstance()->RequestNewLevel(); // ActorWorld破棄は既存のUpdate safe pointまで延期する。
		}

	private:
		enum class State
		{
			Idle,
			WaitingForEmptyLevel,
			WaitingForSave,
		};

		EditorSceneDeferredController() = default;
		SceneManager* sceneManager_ = nullptr;
		State state_ = State::Idle;
		bool savePopupObserved_ = false;
	};
} // namespace Ken4lowEngine
