#pragma once

namespace Ken4lowEngine
{
	enum class EditorPlayState
	{
		Edit,
		Play,
		Pause,
	};

	enum class EditorInputMode
	{
		Editor,
		GameCaptured,
		GameReleased,
	};

	/// <summary>Draw中のボタン操作を次の安全なUpdate地点へ渡すPIE要求です。</summary>
	enum class EditorPlayRequest
	{
		None,
		Start,
		Resume,
		Pause,
		Stop,
		Step,
		KeepChangesAndStop,
	};

	/// <summary>
	/// EditorのPlay状態、Main Viewport入力、PIE Sessionへの遅延要求を管理します。
	/// </summary>
	class EditorPlayController
	{
	public:
		static EditorPlayController* GetInstance();

		void Play();
		void Pause();
		void Stop();
		void Step();
		void KeepChangesAndStop();
		void ToggleInputCapture();
		void CaptureGameInput();
		void ReleaseGameInput();
		void ForceReleaseToEditor();
		void SetDebugFreezeEnabled(bool enabled);

		EditorPlayRequest ConsumePendingRequest();
		void CommitPlayStarted();
		void CommitPlayResumed();
		void CommitPaused();
		void CommitStopped();
		void RejectPendingRequest();

		bool IsEditing() const;
		bool IsPlaying() const;
		bool IsPaused() const;
		bool IsRuntimeSessionActive() const;
		bool IsTransitionPending() const { return pendingRequest_ != EditorPlayRequest::None; }
		bool IsGameCaptured() const;
		bool IsGameReleased() const;
		bool IsEditorInputMode() const;
		bool IsDebugFreezeEnabled() const { return debugFreezeEnabled_; }

		EditorPlayState GetPlayState() const { return playState_; }
		EditorInputMode GetInputMode() const { return inputMode_; }
		EditorPlayRequest GetPendingRequest() const { return pendingRequest_; }
		const char* GetPlayStateText() const;
		const char* GetInputModeText() const;
		const char* GetInputStatusText() const;
		const char* GetDebugFreezeStatusText() const;
		const char* GetPendingRequestText() const;

	private:
		EditorPlayController() = default;
		~EditorPlayController() = default;
		EditorPlayController(const EditorPlayController&) = delete;
		EditorPlayController& operator=(const EditorPlayController&) = delete;

		void QueueRequest(EditorPlayRequest request);

		EditorPlayState playState_ = EditorPlayState::Edit; // 起動直後はEditor Worldを表示する。
		EditorInputMode inputMode_ = EditorInputMode::GameReleased; // 起動直後はEditor操作を優先する。
		EditorPlayRequest pendingRequest_ = EditorPlayRequest::None; // GPU Resource破棄をDraw中に行わないためUpdateまで遅延する。
		bool debugFreezeEnabled_ = false;
	};
} // namespace Ken4lowEngine
