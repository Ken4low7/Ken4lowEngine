#pragma once

#include "EditorContext.h"
#include "EditorLevelService.h"
#include "EditorPlayController.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>
	/// New LevelとOpen Levelを描画中には実行せず、次フレームのUpdate開始時まで遅延します。
	/// </summary>
	class EditorLevelDeferredController
	{
	public:
		static EditorLevelDeferredController* GetInstance()
		{
			static EditorLevelDeferredController instance;
			return &instance;
		}

		void RequestNewLevel()
		{
			RequestDestructiveAction(PendingAction::NewLevel, {});
		}

		void RequestOpenLevelDialog()
		{
			if (!CanRequestLevelChange()) return;
			requestOpenDialog_ = true;
		}

		void RequestOpenLevelPath(const std::filesystem::path& path)
		{
			RequestDestructiveAction(PendingAction::OpenLevel, NormalizeLevelPath(path));
		}

		/// <summary>GPUコマンドを記録していないUpdateフェーズで予約済みLevel操作を実行します。</summary>
		void ProcessSafePoint()
		{
			if (!executeAtSafePoint_ || pendingAction_ == PendingAction::None) return;

			const PendingAction action = pendingAction_;
			const std::filesystem::path openPath = pendingOpenPath_;
			executeAtSafePoint_ = false;
			pendingAction_ = PendingAction::None;
			pendingOpenPath_.clear();
			waitingForSaveBeforeAction_ = false;
			saveAsPopupObserved_ = false;

			EditorContext::GetInstance()->MarkLevelDirty(false); // Service側の確認Popupを通さず安全なUpdate地点で破棄処理へ進める。
			EditorLevelService* levelService = EditorLevelService::GetInstance();
			if (action == PendingAction::NewLevel)
			{
				levelService->RequestNewLevel();
			}
			else if (action == PendingAction::OpenLevel)
			{
				levelService->RequestOpenLevelPath(openPath);
			}
		}

#ifdef USE_IMGUI
		void UpdateShortcuts()
		{
			const ImGuiIO& io = ImGui::GetIO();
			if (io.WantTextInput || ImGui::IsAnyItemActive() || !io.KeyCtrl) return;

			if (ImGui::IsKeyPressed(ImGuiKey_N, false)) RequestNewLevel();
			if (ImGui::IsKeyPressed(ImGuiKey_O, false)) RequestOpenLevelDialog();
			if (ImGui::IsKeyPressed(ImGuiKey_S, false))
			{
				if (io.KeyShift) EditorLevelService::GetInstance()->RequestSaveLevelAs();
				else EditorLevelService::GetInstance()->RequestSaveLevel();
			}
		}

		void DrawFileMenuItems()
		{
			EditorLevelService* levelService = EditorLevelService::GetInstance();
			const bool canEdit = EditorPlayController::GetInstance()->IsEditing();
			if (ImGui::MenuItem("New Level", "Ctrl+N", false, canEdit)) RequestNewLevel();
			if (ImGui::MenuItem("Open Level...", "Ctrl+O", false, canEdit)) RequestOpenLevelDialog();
			if (ImGui::MenuItem("Save Level", "Ctrl+S", false, canEdit)) levelService->RequestSaveLevel();
			if (ImGui::MenuItem("Save Level As...", "Ctrl+Shift+S", false, canEdit)) levelService->RequestSaveLevelAs();

			const std::vector<std::string>& recentLevels = levelService->GetRecentLevels();
			if (ImGui::BeginMenu("Recent Levels", !recentLevels.empty() && canEdit))
			{
				for (const std::string& recentPath : recentLevels)
				{
					const std::filesystem::path path(recentPath);
					const std::string label = path.filename().string() + "##DeferredRecent" + recentPath;
					if (ImGui::MenuItem(label.c_str())) RequestOpenLevelPath(path);
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", recentPath.c_str());
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();
			ImGui::TextDisabled(
				"Auto Save: %s / %.0f sec",
				levelService->IsAutoSaveEnabled() ? "ON" : "OFF",
				levelService->GetAutoSaveIntervalSeconds());
		}

		void DrawDialogs()
		{
			if (requestOpenDialog_)
			{
				RefreshLevelFileList();
				ImGui::OpenPopup("Open Level##DeferredLevelOpen");
				requestOpenDialog_ = false;
			}
			if (requestUnsavedDialog_)
			{
				ImGui::OpenPopup("Unsaved Level##DeferredLevelUnsaved");
				requestUnsavedDialog_ = false;
			}

			DrawOpenDialog();
			DrawUnsavedDialog();
			EditorLevelService::GetInstance()->DrawDialogs();
			UpdateSaveContinuation();
		}
#endif

	private:
		enum class PendingAction
		{
			None,
			NewLevel,
			OpenLevel,
		};

		static inline const std::filesystem::path kLevelDirectory = "Resources/JSON/Levels";

		EditorLevelDeferredController() = default;
		~EditorLevelDeferredController() = default;
		EditorLevelDeferredController(const EditorLevelDeferredController&) = delete;
		EditorLevelDeferredController& operator=(const EditorLevelDeferredController&) = delete;

		bool CanRequestLevelChange() const
		{
			return EditorPlayController::GetInstance()->IsEditing();
		}

		void RequestDestructiveAction(PendingAction action, const std::filesystem::path& path)
		{
			if (!CanRequestLevelChange()) return;
			pendingAction_ = action;
			pendingOpenPath_ = path;
			executeAtSafePoint_ = false;
			waitingForSaveBeforeAction_ = false;
			saveAsPopupObserved_ = false;

			if (EditorContext::GetInstance()->IsLevelDirty())
			{
				requestUnsavedDialog_ = true;
				return;
			}
			executeAtSafePoint_ = true; // ImGuiフレーム中は予約だけ立て、ActorWorldの破棄は行わない。
		}

		void CancelPendingAction()
		{
			pendingAction_ = PendingAction::None;
			pendingOpenPath_.clear();
			executeAtSafePoint_ = false;
			waitingForSaveBeforeAction_ = false;
			saveAsPopupObserved_ = false;
		}

		static std::filesystem::path NormalizeLevelPath(const std::filesystem::path& requestedPath)
		{
			if (requestedPath.empty()) return {};
			std::filesystem::path path = requestedPath;
			if (!path.has_extension()) path.replace_extension(".json");
			if (!path.has_parent_path()) path = kLevelDirectory / path;
			return path.lexically_normal();
		}

#ifdef USE_IMGUI
		void RefreshLevelFileList()
		{
			levelFiles_.clear();
			std::error_code error;
			std::filesystem::create_directories(kLevelDirectory, error);
			for (const auto& entry : std::filesystem::recursive_directory_iterator(kLevelDirectory, error))
			{
				if (error) break;
				if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
				levelFiles_.push_back(entry.path());
			}
			std::sort(levelFiles_.begin(), levelFiles_.end());
		}

		void DrawOpenDialog()
		{
			if (!ImGui::BeginPopupModal("Open Level##DeferredLevelOpen", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
			ImGui::TextDisabled("開くKen4lowLevel JSONを選択してください。");
			ImGui::SetNextItemWidth(560.0f);
			ImGui::InputText("Path##DeferredOpenLevelPath", openPathBuffer_.data(), openPathBuffer_.size());

			if (ImGui::BeginChild("##DeferredLevelFileList", ImVec2(560.0f, 260.0f), true))
			{
				for (std::size_t index = 0; index < levelFiles_.size(); ++index)
				{
					const std::filesystem::path& path = levelFiles_[index];
					const bool selected = selectedLevelFileIndex_ == static_cast<int>(index);
					const std::string label = path.lexically_relative(kLevelDirectory).generic_string() + "##DeferredLevelFile" + std::to_string(index);
					if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
					{
						selectedLevelFileIndex_ = static_cast<int>(index);
						std::snprintf(openPathBuffer_.data(), openPathBuffer_.size(), "%s", path.generic_string().c_str());
						if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
						{
							ImGui::CloseCurrentPopup();
							RequestOpenLevelPath(path);
						}
					}
				}
			}
			ImGui::EndChild();

			if (ImGui::Button("Open", ImVec2(120.0f, 0.0f)))
			{
				const std::filesystem::path path = NormalizeLevelPath(openPathBuffer_.data());
				ImGui::CloseCurrentPopup();
				RequestOpenLevelPath(path);
			}
			ImGui::SameLine();
			if (ImGui::Button("Refresh", ImVec2(120.0f, 0.0f))) RefreshLevelFileList();
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
		}

		void DrawUnsavedDialog()
		{
			if (!ImGui::BeginPopupModal("Unsaved Level##DeferredLevelUnsaved", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
			ImGui::Text("%s has unsaved changes.", EditorContext::GetInstance()->GetActiveLevelName().c_str());
			ImGui::TextDisabled("続行する前に保存しますか？");
			ImGui::Separator();

			if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
			{
				EditorLevelService* levelService = EditorLevelService::GetInstance();
				if (levelService->GetCurrentLevelPath().empty())
				{
					waitingForSaveBeforeAction_ = true;
					saveAsPopupObserved_ = false;
					levelService->RequestSaveLevelAs();
					ImGui::CloseCurrentPopup();
				}
				else
				{
					levelService->RequestSaveLevel();
					if (!EditorContext::GetInstance()->IsLevelDirty())
					{
						executeAtSafePoint_ = true;
						ImGui::CloseCurrentPopup();
					}
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save", ImVec2(120.0f, 0.0f)))
			{
				EditorContext::GetInstance()->MarkLevelDirty(false);
				executeAtSafePoint_ = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				CancelPendingAction();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		void UpdateSaveContinuation()
		{
			if (!waitingForSaveBeforeAction_) return;
			const bool saveAsOpen = ImGui::IsPopupOpen("Save Level As##EditorLevelSaveAs");
			if (saveAsOpen) saveAsPopupObserved_ = true;

			if (!EditorContext::GetInstance()->IsLevelDirty())
			{
				waitingForSaveBeforeAction_ = false;
				saveAsPopupObserved_ = false;
				executeAtSafePoint_ = true;
			}
			else if (saveAsPopupObserved_ && !saveAsOpen)
			{
				CancelPendingAction(); // Save Asをキャンセルした場合はNew/Open予約も破棄する。
			}
		}
#endif

		PendingAction pendingAction_ = PendingAction::None;
		std::filesystem::path pendingOpenPath_;
		bool executeAtSafePoint_ = false;
		bool requestOpenDialog_ = false;
		bool requestUnsavedDialog_ = false;
		bool waitingForSaveBeforeAction_ = false;
		bool saveAsPopupObserved_ = false;
		std::vector<std::filesystem::path> levelFiles_;
		std::array<char, 512> openPathBuffer_{};
		int selectedLevelFileIndex_ = -1;
	};
} // namespace Ken4lowEngine
