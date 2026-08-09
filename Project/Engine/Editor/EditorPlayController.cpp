#include "EditorPlayController.h"
#include "EditorCommandHistory.h"

namespace Ken4lowEngine
{
	EditorPlayController* EditorPlayController::GetInstance()
	{
		static EditorPlayController instance;
		return &instance;
	}

	void EditorPlayController::QueueRequest(EditorPlayRequest request)
	{
		pendingRequest_ = request; // UI Draw中は状態を直接切り替えずSceneManagerの安全地点へ要求だけ渡す。
	}

	void EditorPlayController::Play()
	{
		if (playState_ == EditorPlayState::Edit)
		{
			QueueRequest(EditorPlayRequest::Start);
		}
		else if (playState_ == EditorPlayState::Pause)
		{
			QueueRequest(EditorPlayRequest::Resume);
		}
	}

	void EditorPlayController::Pause()
	{
		if (playState_ == EditorPlayState::Play) QueueRequest(EditorPlayRequest::Pause);
	}

	void EditorPlayController::Stop()
	{
		if (playState_ != EditorPlayState::Edit || pendingRequest_ == EditorPlayRequest::Start)
		{
			QueueRequest(EditorPlayRequest::Stop);
		}
	}

	void EditorPlayController::Step()
	{
		if (playState_ == EditorPlayState::Pause) QueueRequest(EditorPlayRequest::Step);
	}

	void EditorPlayController::KeepChangesAndStop()
	{
		if (playState_ != EditorPlayState::Edit) QueueRequest(EditorPlayRequest::KeepChangesAndStop);
	}

	EditorPlayRequest EditorPlayController::ConsumePendingRequest()
	{
		const EditorPlayRequest request = pendingRequest_;
		pendingRequest_ = EditorPlayRequest::None;
		return request;
	}

	void EditorPlayController::CommitPlayStarted()
	{
		EditorCommandHistory::GetInstance()->Clear(); // Runtime WorldはEditor Command履歴と別ライフタイムにする。
		playState_ = EditorPlayState::Play;
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::CommitPlayResumed()
	{
		playState_ = EditorPlayState::Play;
		inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::CommitPaused()
	{
		playState_ = EditorPlayState::Pause;
		inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::CommitStopped()
	{
		playState_ = EditorPlayState::Edit;
		inputMode_ = EditorInputMode::GameReleased;
		pendingRequest_ = EditorPlayRequest::None;
	}

	void EditorPlayController::RejectPendingRequest()
	{
		pendingRequest_ = EditorPlayRequest::None;
		if (playState_ == EditorPlayState::Edit) inputMode_ = EditorInputMode::GameReleased;
	}

	void EditorPlayController::ToggleInputCapture()
	{
		if (IsGameCaptured())
		{
			ReleaseGameInput();
			return;
		}
		CaptureGameInput();
	}

	void EditorPlayController::CaptureGameInput()
	{
		if (IsRuntimeSessionActive()) inputMode_ = EditorInputMode::GameCaptured;
	}

	void EditorPlayController::ReleaseGameInput() { inputMode_ = EditorInputMode::GameReleased; }
	void EditorPlayController::ForceReleaseToEditor() { ReleaseGameInput(); }
	void EditorPlayController::SetDebugFreezeEnabled(bool enabled) { debugFreezeEnabled_ = enabled; }
	bool EditorPlayController::IsEditing() const { return playState_ == EditorPlayState::Edit; }
	bool EditorPlayController::IsPlaying() const { return playState_ == EditorPlayState::Play; }
	bool EditorPlayController::IsPaused() const { return playState_ == EditorPlayState::Pause; }
	bool EditorPlayController::IsRuntimeSessionActive() const { return playState_ != EditorPlayState::Edit; }
	bool EditorPlayController::IsGameCaptured() const { return inputMode_ == EditorInputMode::GameCaptured; }
	bool EditorPlayController::IsGameReleased() const { return inputMode_ == EditorInputMode::GameReleased; }
	bool EditorPlayController::IsEditorInputMode() const { return inputMode_ == EditorInputMode::Editor; }

	const char* EditorPlayController::GetPlayStateText() const
	{
		switch (playState_)
		{
		case EditorPlayState::Play: return "再生中";
		case EditorPlayState::Pause: return "一時停止";
		case EditorPlayState::Edit:
		default: return "編集中";
		}
	}

	const char* EditorPlayController::GetInputModeText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured: return "ゲーム入力取得";
		case EditorInputMode::GameReleased: return "エディターへ解放";
		case EditorInputMode::Editor:
		default: return "エディター入力";
		}
	}

	const char* EditorPlayController::GetInputStatusText() const
	{
		switch (inputMode_)
		{
		case EditorInputMode::GameCaptured: return "入力: ゲーム操作";
		case EditorInputMode::GameReleased: return "入力: エディターへ解放";
		case EditorInputMode::Editor:
		default: return "入力: エディター操作";
		}
	}

	const char* EditorPlayController::GetDebugFreezeStatusText() const
	{
		return debugFreezeEnabled_ ? "デバッグ停止: 有効" : "デバッグ停止: 無効";
	}

	const char* EditorPlayController::GetPendingRequestText() const
	{
		switch (pendingRequest_)
		{
		case EditorPlayRequest::Start: return "PIE開始待ち";
		case EditorPlayRequest::Resume: return "再開待ち";
		case EditorPlayRequest::Pause: return "一時停止待ち";
		case EditorPlayRequest::Stop: return "停止待ち";
		case EditorPlayRequest::Step: return "1フレーム実行待ち";
		case EditorPlayRequest::KeepChangesAndStop: return "変更反映停止待ち";
		case EditorPlayRequest::None:
		default: return "";
		}
	}
} // namespace Ken4lowEngine
