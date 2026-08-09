#include "EditorModeController.h"

#include "EditorCommandHistory.h"
#include "EditorContext.h"
#include "EditorLevelDeferredController.h"
#include "EditorPlayController.h"
#include "EditorWindowManager.h"
#include <Input.h>
#include <Wireframe.h>

#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	EditorModeController* EditorModeController::GetInstance()
	{
		static EditorModeController instance;
		return &instance;
	}

	void EditorModeController::Initialize()
	{
#ifdef _DEBUG
		editorModeEnabled_ = true;
#else
		editorModeEnabled_ = false;
#endif
		ApplyModeSideEffects();
	}

	void EditorModeController::Update(Input* input)
	{
#ifdef _DEBUG
		EditorLevelDeferredController::GetInstance()->ProcessSafePoint(); // 前フレームのGPU完了後かつ新しいDraw開始前にLevel破棄・復元を行う。
		EditorPlayController* playController = EditorPlayController::GetInstance();

		if (input != nullptr && input->TriggerRawKey(DIK_F1))
		{
			if (playController->IsRuntimeSessionActive() || playController->GetPendingRequest() == EditorPlayRequest::Start)
			{
				EditorWindowManager::GetInstance()->AddOutputLog(
					EditorLogLevel::Warning,
					"PIE Runtime Worldを保護するためF1切り替えを無効にしました。先にStopしてください。");
			}
			else
			{
				SetEditorModeEnabled(!editorModeEnabled_); // PIE外だけEditor / Game Previewを切り替える。
			}
		}

		if (input != nullptr && IsEditorModeEnabled() && playController->IsEditing())
		{
			bool allowHistoryShortcut = true;
#ifdef USE_IMGUI
			allowHistoryShortcut = !ImGui::GetIO().WantTextInput;
#endif
			if (allowHistoryShortcut)
			{
				const bool control = input->PushRawKey(DIK_LCONTROL) || input->PushRawKey(DIK_RCONTROL);
				const bool shift = input->PushRawKey(DIK_LSHIFT) || input->PushRawKey(DIK_RSHIFT);
				EditorCommandHistory* history = EditorCommandHistory::GetInstance();
				bool changed = false;
				if (control && input->TriggerRawKey(DIK_Z)) changed = shift ? history->Redo() : history->Undo();
				else if (control && input->TriggerRawKey(DIK_Y)) changed = history->Redo();
				if (changed)
				{
					EditorContext::GetInstance()->MarkLevelDirty();
					EditorWindowManager::GetInstance()->AddOutputLog(EditorLogLevel::Info, "[Editor] Undo / Redoを実行しました。");
				}
			}
		}

		if (input != nullptr && IsEditorModeEnabled() && playController->IsPlaying() &&
			playController->IsGameCaptured() && input->TriggerRawKey(DIK_ESCAPE))
		{
			playController->ReleaseGameInput();
		}

		if (input != nullptr && IsEditorModeEnabled() && playController->IsGameReleased())
		{
			input->SetGameInputEnabled(false);
			input->SetLockCursor(false);
			input->SetCursorVisible(true);
		}
#else
		(void)input;
		SetEditorModeEnabled(false);
#endif
	}

	void EditorModeController::SetEditorModeEnabled(bool enabled)
	{
#ifndef _DEBUG
		enabled = false;
#endif
		if (editorModeEnabled_ == enabled)
		{
			ApplyModeSideEffects();
			return;
		}

		if (!enabled && EditorPlayController::GetInstance()->IsRuntimeSessionActive())
		{
			return; // PIE中にEditor UIだけ消えてStop不能になる状態を防ぐ。
		}

		editorModeEnabled_ = enabled;
		if (!editorModeEnabled_) EditorCommandHistory::GetInstance()->Clear();
		ApplyModeSideEffects();
		NotifyModeChanged();
	}

	bool EditorModeController::IsEditorModeEnabled() const
	{
#ifdef _DEBUG
		return editorModeEnabled_;
#else
		return false;
#endif
	}

	void EditorModeController::ApplyModeSideEffects()
	{
		Wireframe::GetInstance()->SetDebugDrawEnabled(ShouldDrawDebugVisuals());
		if (Input* input = Input::GetInstance())
		{
			if (IsGamePreviewMode())
			{
				input->SetGameInputEnabled(true);
				input->ClearEditorViewportMouseOverride();
			}
		}
	}

	void EditorModeController::NotifyModeChanged() const
	{
#ifdef USE_IMGUI
		const char* modeName = IsEditorModeEnabled() ? "Editor Mode" : "Game Preview Mode";
		EditorWindowManager::GetInstance()->AddOutputLog(EditorLogLevel::Info, std::string("[Editor] Switched to ") + modeName + " (F1).");
#endif
	}
} // namespace Ken4lowEngine
