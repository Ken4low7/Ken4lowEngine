#pragma once

#include "EditorContext.h"
#include "EditorLevelDeferredController.h"
#include "EditorLevelService.h"
#include "EditorPlayController.h"

#include <SceneManager.h>

#include <filesystem>
#include <string>

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
			if (ImGui::MenuItem("New Scene...", "Ctrl+Alt+N", false, canCreate)) RequestNewScene();
			ImGui::TextDisabled("Scene = Level JSON / Runtime Class = DataDrivenScene");
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
