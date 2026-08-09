#include "GamePlayFlow.h"

#include <Input.h>
#include <SceneManager.h>

#include "HUDManager.h"
#include "Player.h"

#ifdef USE_IMGUI
#include <Editor/EditorPlayController.h>
#endif

using namespace Ken4lowEngine;

namespace
{
	void ApplyGameCursorMode(Ken4lowEngine::Input* input, bool fpsCapture)
	{
		if (!input) { return; }

		// UI状態ではカーソルを表示し、通常プレイへ戻る時だけFPS用にロック/非表示へ戻す。
		input->SetLockCursor(fpsCapture);
		input->SetCursorVisible(!fpsCapture);
	}

	void RestoreEditorGameCapture()
	{
#ifdef USE_IMGUI
		EditorPlayController::GetInstance()->CaptureGameInput(); // Pause/Resultからゲームへ戻る時はEditor側の入力モードもGameCapturedへ戻す。
#endif
	}
}

/// -------------------------------------------------------------
///				　				 初期化処理
/// -------------------------------------------------------------
void GamePlayFlow::Initialize()
{
	// ポーズメニューの初期化
	pauseMenu_ = std::make_unique<PauseMenu>();
	pauseMenu_->Initialize();

	// 結果画面の初期化
	resultMenu_ = std::make_unique<ResultMenu>();
	resultMenu_->Initialize();

	isPaused_ = false;				// 初期状態は非ポーズ
	resultInputCooldown_ = 0.0f;	// クールタイムは0で初期化
	state_ = State::Playing;		// デフォルトはPlaying状態
}

/// -------------------------------------------------------------
///				　				 終了処理
/// -------------------------------------------------------------
void GamePlayFlow::Finalize()
{
	pauseMenu_.reset();
	resultMenu_.reset();

	isPaused_ = false;
	resultInputCooldown_ = 0.0f;
	state_ = State::Playing;
}

/// -------------------------------------------------------------
///			新しいゲームの開始に向けて状態をリセットする
/// -------------------------------------------------------------
void GamePlayFlow::ResetForNewGame(bool startIntro)
{
	// ポーズキャンセル
	CancelPause();

	// クールタイムリセット
	resultInputCooldown_ = 0.0f;

	// 状態を判定
	state_ = startIntro ? State::Intro : State::Playing;

	if (!startIntro)
	{
		if (Input* input = Input::GetInstance())
		{
			input->SetGameInputEnabled(true);
			ApplyGameCursorMode(input, true);
		}
		RestoreEditorGameCapture();
	}
}

/// -------------------------------------------------------------
///				　				 ゲームプレイ開始
/// -------------------------------------------------------------
void GamePlayFlow::StartPlaying()
{
	// ポーズキャンセル
	CancelPause();

	// クールタイムリセット
	resultInputCooldown_ = 0.0f;

	// 状態を Playing にする
	state_ = State::Playing;
	if (Input* input = Input::GetInstance())
	{
		input->SetGameInputEnabled(true);
		ApplyGameCursorMode(input, true);
	}
	RestoreEditorGameCapture();
}

/// -------------------------------------------------------------
///				　				 ポーズ開始
/// -------------------------------------------------------------
void GamePlayFlow::EnterPause(Ken4lowEngine::Input* input)
{
	if (isPaused_) { return; }
	if (state_ != State::Playing) { return; }

	// ポーズ状態にする
	isPaused_ = true;

	// ポーズメニューを開く
	if (pauseMenu_)	pauseMenu_->Open();

	ApplyGameCursorMode(input, false);
}

/// -------------------------------------------------------------
///				　				 ポーズ解除
/// -------------------------------------------------------------
void GamePlayFlow::ExitPause(Ken4lowEngine::Input* input, bool lockCursorOnResume)
{
	if (!isPaused_) { return; }

	// 非ポーズ状態にする
	isPaused_ = false;

	// ポーズメニューを閉じる
	if (pauseMenu_) pauseMenu_->Close();

	if (input) input->SetGameInputEnabled(true);
	ApplyGameCursorMode(input, lockCursorOnResume);
	RestoreEditorGameCapture();
}

/// -------------------------------------------------------------
///				　				 ポーズキャンセル
/// -------------------------------------------------------------
void GamePlayFlow::CancelPause()
{
	if (!isPaused_) { return; }

	// 非ポーズ状態にする
	isPaused_ = false;

	// ポーズを閉じる
	if (pauseMenu_)	pauseMenu_->Close();
}

/// -------------------------------------------------------------
///				　			ゲームクリア開始
/// -------------------------------------------------------------
void GamePlayFlow::EnterGameClear(Ken4lowEngine::Input* input, const std::function<void()>& onUnlockNextStage)
{
	if (state_ == State::GameClear) { return; }

	if (onUnlockNextStage)
	{
		onUnlockNextStage();
	}

	state_ = State::GameClear;
	isPaused_ = false;
	resultInputCooldown_ = 0.25f;

	if (pauseMenu_)
	{
		pauseMenu_->Close();
	}

	if (resultMenu_)
	{
		resultMenu_->Open(ResultMenuMode::GameClear);
	}

	ApplyGameCursorMode(input, false);
}

/// -------------------------------------------------------------
///				　			  ゲームオーバー開始
/// -------------------------------------------------------------
void GamePlayFlow::EnterGameOver(Ken4lowEngine::Input* input)
{
	if (state_ == State::GameOver) { return; }

	state_ = State::GameOver;
	isPaused_ = false;
	resultInputCooldown_ = 0.25f;

	if (pauseMenu_)	pauseMenu_->Close();

	if (resultMenu_) resultMenu_->Open(ResultMenuMode::GameOver);

	ApplyGameCursorMode(input, false);
}

/// -------------------------------------------------------------
///				　			ポーズ中の更新
/// -------------------------------------------------------------
void GamePlayFlow::UpdatePaused(const PausedUpdateContext& ctx)
{
	if (ctx.hud && ctx.player)
	{
		ctx.hud->SetHP(ctx.player->GetHP(), ctx.player->GetMaxHP());
		ctx.hud->Update(ctx.deltaTime);
	}

	if (!pauseMenu_ || !ctx.input)
	{
		return;
	}

	ctx.input->SetGameInputEnabled(true); // EditorがGameReleasedでもPause UIのViewport座標と入力だけは有効にする。
	const PauseMenuCommand cmd = pauseMenu_->Update(ctx.input);

	switch (cmd)
	{
	case PauseMenuCommand::Resume:
		ExitPause(ctx.input, ctx.lockCursorOnResume);
		break;

	case PauseMenuCommand::ToStageSelect:
		ApplyGameCursorMode(ctx.input, false);
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("StageSelectScene");
		}
		break;

	case PauseMenuCommand::ToTitle:
		ApplyGameCursorMode(ctx.input, false);
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("TitleScene");
		}
		break;

	case PauseMenuCommand::None:
	default:
		break;
	}
}

/// -------------------------------------------------------------
///				　			結果画面の更新
/// -------------------------------------------------------------
void GamePlayFlow::UpdateResult(const ResultUpdateContext& ctx)
{
	if (resultInputCooldown_ > 0.0f)
	{
		resultInputCooldown_ -= ctx.deltaTime;
		if (resultInputCooldown_ < 0.0f)
		{
			resultInputCooldown_ = 0.0f;
		}
	}

	if (!ctx.input || resultInputCooldown_ > 0.0f)
	{
		return;
	}

	ctx.input->SetGameInputEnabled(true); // Result UIもEditorのGameReleased状態に依存せずHover/Click/決定を受け取る。
	ResultMenuCommand cmd = ResultMenuCommand::None;
	if (resultMenu_)
	{
		cmd = resultMenu_->Update(ctx.input);
	}

	switch (cmd)
	{
	case ResultMenuCommand::NextStage:

		if (ctx.onNextStage)
		{
			ctx.onNextStage();
		}

		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("StageSelectScene");
		}
		return;

	case ResultMenuCommand::Retry:
		if (ctx.onRetry)
		{
			ctx.onRetry();
		}
		return;

	case ResultMenuCommand::ToTitle:
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("TitleScene");
		}
		return;

	case ResultMenuCommand::None:
	default:
		break;
	}

	if (ctx.input->TriggerRawKey(DIK_R))
	{
		if (ctx.onRetry)
		{
			ctx.onRetry();
		}
		return;
	}

	if (ctx.input->TriggerRawKey(DIK_T))
	{
		if (ctx.sceneManager)
		{
			ctx.sceneManager->ChangeScene("TitleScene");
		}
		return;
	}
}

/// -------------------------------------------------------------
///				　				 UIの描画
/// -------------------------------------------------------------
void GamePlayFlow::DrawUI()
{
	if (isPaused_ && pauseMenu_)
	{
		pauseMenu_->Draw();
	}

	if (IsResultState() && resultMenu_)
	{
		resultMenu_->Update();
		resultMenu_->Draw();
	}
}
