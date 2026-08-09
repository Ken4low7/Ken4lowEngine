#define NOMINMAX
#include "GamePlayScene.h"
#include <Input.h>
#include <SceneManager.h>
#include <GameTimer.h>
#include <Player.h>
#include <string_view>
#include <utility>
#include <Editor/EditorTransformAccess.h>
#include <CameraManager.h>
#include "StageRepository.h"

#ifdef USE_IMGUI
#include <Editor/EditorModeController.h>
#include <imgui.h>
#endif

#ifdef _DEBUG
#include <DebugCamera.h>
#endif

using namespace Ken4lowEngine;

K4E::EditorInputPolicy GamePlayScene::GetEditorInputPolicy() const
{
	// UI操作が必要なポーズ/リザルト中はDebug Editor側にもカーソル表示モードを通知する。
	if (flow_ && (flow_->IsPaused() || flow_->IsResultState()))
	{
		return K4E::EditorInputPolicy::UiMouse;
	}

	return K4E::EditorInputPolicy::FpsCapture;
}

/// -------------------------------------------------------------
/// 初期化
/// -------------------------------------------------------------
void GamePlayScene::Initialize()
{
	InitializeSystems();

	loadStep_ = 0;
	isLoadReady_ = false;

	flow_.reset();
	stageContext_.reset();
	world_.reset();
	introDirector_.reset();
	debugTools_.reset();
	frustumCullingDebug_.reset();
	uiController_.reset();
	debugWindow_.reset();
	bulletDecalManager_.reset();
	if (effectController_)
	{
		effectController_->Finalize(world_.get());
		effectController_.reset();
	}

	if (!fadeManager_)
	{
		fadeManager_ = std::make_unique<FadeManager>();
		fadeManager_->Initialize();
	}

	uiController_ = std::make_unique<GamePlayUIController>();
	debugWindow_ = std::make_unique<GamePlayDebugWindow>();
}

/// -------------------------------------------------------------
/// エンジン依存システム初期化
/// 
/// - DirectXCommon / Input 取得
/// - カーソルロック
/// - デバッグカメラ初期化
/// -------------------------------------------------------------
void GamePlayScene::InitializeSystems()
{
#ifdef _DEBUG
	K4E::DebugCamera::GetInstance()->Initialize();
#endif
	input_ = K4E::Input::GetInstance();

	if (input_)
	{
		// ゲームプレイ開始時はカーソルをロックして非表示
		input_->SetLockCursor(true);
		input_->SetCursorVisible(false);
	}
}

/// -------------------------------------------------------------
/// ゲームプレイ構成オブジェクト生成
/// -------------------------------------------------------------
void GamePlayScene::InitializeGameplayObjects(bool skipStage1Tutorial)
{
	flow_ = std::make_unique<GamePlayFlow>();
	flow_->Initialize();

	stageContext_ = std::make_unique<GamePlayStageContext>();
	stageContext_->InitializeFromRepository();

	world_ = std::make_unique<GamePlayWorld>();
	world_->Initialize(*stageContext_, skipStage1Tutorial);
	InitializeBulletDecals();

	introDirector_ = std::make_unique<GamePlayIntroDirector>();

	debugTools_ = std::make_unique<GamePlayDebugTools>();
	debugTools_->Initialize();

	frustumCullingDebug_ = std::make_unique<FrustumCullingDebugController>();
	frustumCullingDebug_->Initialize();

	uiController_ = std::make_unique<GamePlayUIController>();
	debugWindow_ = std::make_unique<GamePlayDebugWindow>();
	InitializeEffectController();

	if (!fadeManager_)
	{
		fadeManager_ = std::make_unique<FadeManager>();
		fadeManager_->Initialize();
	}
}


void GamePlayScene::InitializeEffectController()
{
	effectController_ = std::make_unique<GamePlayEffectController>();
	effectController_->Initialize(world_.get());
}

void GamePlayScene::InitializeBulletDecals()
{
	FinalizeBulletDecals();
	bulletDecalManager_ = std::make_unique<BulletDecalManager>();
	if (!bulletDecalManager_->Initialize())
	{
		// 描画リソースを作れない環境でも射撃処理を止めず、銃痕だけを無効化する。
		bulletDecalManager_.reset();
		return;
	}

	if (world_ && world_->GetBulletManager())
	{
		world_->GetBulletManager()->SetWorldImpactCallback([this](const K4E::Vector3& position, const K4E::Vector3& normal)
			{
				if (bulletDecalManager_)
				{
					bulletDecalManager_->AddDecal(position, normal);
				}
			});
	}
}

void GamePlayScene::FinalizeBulletDecals()
{
	if (world_ && world_->GetBulletManager())
	{
		// Worldより先に通知を解除し、破棄済みManagerへのコールバックを残さない。
		world_->GetBulletManager()->SetWorldImpactCallback({});
	}
	if (bulletDecalManager_)
	{
		bulletDecalManager_->Finalize();
		bulletDecalManager_.reset();
	}
}

/// -------------------------------------------------------------
/// 新規ゲーム開始準備
/// 
/// - イントロをリセット
/// - イントロ有無を flow に反映
/// - イントロが無い場合は即 Wave 開始
/// -------------------------------------------------------------
void GamePlayScene::SetupNewGame(bool skipIntro)
{
	if (!introDirector_ || !stageContext_ || !flow_)
	{
		return;
	}

	introDirector_->Reset(*stageContext_, 2.5f);

	const bool hasIntroPoints = introDirector_->HasIntro();
	const bool startIntro = hasIntroPoints && !skipIntro;

	flow_->ResetForNewGame(startIntro);

	if (world_)
	{
		if (startIntro)
		{
			world_->WarmupStartGameplayForIntro();
		}
		else
		{
			world_->SetStartGameplayVisualsVisible(true);
			world_->StartWaves();
		}
	}
}

/// -------------------------------------------------------------
/// 更新
/// 
/// ここでは「順番」と「分岐」だけを担当する。
/// 実処理は private 関数へ逃がして見通しを良くする。
/// -------------------------------------------------------------
void GamePlayScene::Update()
{
	// フレーム開始時に FPSCounter を更新して deltaTime を取得
	const float deltaTime = K4E::GameTimer::GetInstance()->GetDeltaTime();

	// フェードはロード未完了やポーズ中でも見た目を進めたいので、ゲーム進行より先に更新する。
	if (fadeManager_)
	{
		fadeManager_->Update(deltaTime);
	}

	// 段階ロード中にWorldへ触ると未生成サブシステムが混ざるため、通常更新は開始しない。
	if (!isLoadReady_)
	{
		return;
	}

	// 以降は「このフレームでWorldを進めるか」を優先度順に判定する。
	// 早期returnする分岐は、UIや演出だけを進めてゲーム本体を止めるためのもの。
	if (HandleDebugFreeze()) return;

	// リトライ遷移中
	if (UpdateRetryTransition()) return;

	// イントロ更新
	if (UpdateIntro(deltaTime))	return;

	// リザルト更新
	if (UpdateResult(deltaTime)) return;

	// ポーズ切り替え
	if (HandlePauseToggle()) return;

	// ポーズ中更新
	if (UpdatePause(deltaTime))	return;

	// 通常時デバッグ更新
	UpdateDebug();

	// 通常ワールド更新
	UpdateWorld(deltaTime);

	// ゲーム終了判定
	CheckGameEnd();
}


/// -------------------------------------------------------------
/// Editor中の更新
///
/// Edit/Pause中はゲーム進行を止め、フェードなど描画確認に必要な軽い更新だけ行う。
/// -------------------------------------------------------------
void GamePlayScene::UpdateEditor(float deltaTime)
{
	// Editor更新では敵/弾/Wave/Player操作/リザルト遷移を進めない。
	if (fadeManager_)
	{
		fadeManager_->Update(deltaTime);
	}
}

/// -------------------------------------------------------------
/// デバッグ停止処理
/// 
/// F10 で完全停止に入る処理。
/// 停止中は通常更新を進めない。
/// -------------------------------------------------------------
bool GamePlayScene::HandleDebugFreeze()
{
#ifdef _DEBUG
	if (debugTools_ && debugTools_->HandleFreezeToggle(input_, flow_.get()))
	{
		return true;
	}

	if (debugTools_ && debugTools_->IsFrozen())
	{
		debugTools_->UpdateFreeze();
		return true;
	}
#endif

	return false;
}

bool GamePlayScene::UpdateRetryTransition()
{
	if (!isRetryTransitionActive_)
	{
		return false;
	}

	// 画面が完全に覆われてからWorldを破棄/再生成し、再初期化中の見た目が露出しないようにする。
	if (!isRetryRestartDone_)
	{
		if (IsRetryFadeOutFinished())
		{
			RestartGame(true);   // 同じステージをイントロ無しで再構築
			isRetryRestartDone_ = true;

			StartRetryFadeIn();  // ひび割れ→落下で開く
		}

		return true;
	}

	// 再生成後の開く演出が完了したら通常更新へ戻す。
	if (IsRetryFadeInFinished())
	{
		isRetryTransitionActive_ = false;
		isRetryRestartDone_ = false;
	}

	return true;
}

/// -------------------------------------------------------------
/// イントロ更新
/// 
/// イントロ中は通常ゲームプレイ更新を止め、完了フレームだけ通常更新へ戻す。
/// -------------------------------------------------------------
bool GamePlayScene::UpdateIntro(float deltaTime)
{
	if (!(flow_ && (flow_->IsIntro() || flow_->IsEquipIntro())))
	{
		return false;
	}

	if (introDirector_ && stageContext_ && world_)
	{
		introDirector_->Update(
			deltaTime,
			*flow_,
			*stageContext_,
			*world_,
			input_,
			debugTools_ ? debugTools_->IsDebugCamera() : false);
	}

	// 完了フレーム内に通常更新へ進め、終点で1フレーム停止して見えるのを防ぐ。
	return flow_ && (flow_->IsIntro() || flow_->IsEquipIntro());
}

/// -------------------------------------------------------------
/// リザルト更新
/// 
/// リザルト中はリザルト専用フローだけ更新する。
/// -------------------------------------------------------------
bool GamePlayScene::UpdateResult(float deltaTime)
{
	if (!(flow_ && flow_->IsResultState()))
	{
		return false;
	}

	GamePlayFlow::ResultUpdateContext ctx{};
	ctx.deltaTime = deltaTime;
	ctx.input = input_;
	ctx.sceneManager = sceneManager_;

	ctx.onRetry = [this]()
		{
			RequestRetryWithFade();
		};

	ctx.onNextStage = [this]()
		{
			if (stageContext_)
			{
				stageContext_->UnlockNextStage();
			}
		};

	flow_->UpdateResult(ctx);
	return true;
}

/// -------------------------------------------------------------
/// ポーズ切り替え処理
/// 
/// ESC 押下で Pause <-> Resume を切り替える。
/// Resume 時のカーソルロックはデバッグカメラ状態を考慮する。
/// -------------------------------------------------------------
bool GamePlayScene::HandlePauseToggle()
{
	if (!(input_ && input_->TriggerKey(DIK_ESCAPE)))
	{
		return false;
	}

	if (flow_ && flow_->IsPaused())
	{
		const bool lockCursorOnResume =
			!(debugTools_ && debugTools_->IsDebugCamera());

		flow_->ExitPause(input_, lockCursorOnResume);
	}
	else if (flow_)
	{
		flow_->EnterPause(input_);
	}

	return true;
}

/// -------------------------------------------------------------
/// ポーズ中更新
/// 
/// HUD / player / scene manager など必要な情報を
/// Context に詰めて flow 側へ渡す。
/// -------------------------------------------------------------
bool GamePlayScene::UpdatePause(float deltaTime)
{
	if (!(flow_ && flow_->IsPaused()))
	{
		return false;
	}

	GamePlayFlow::PausedUpdateContext ctx{};
	ctx.deltaTime = deltaTime;
	ctx.input = input_;
	ctx.hud = world_ ? world_->GetHUDManager() : nullptr;
	ctx.player = world_ ? world_->GetCharacters().GetPlayer() : nullptr;
	ctx.sceneManager = sceneManager_;
	ctx.lockCursorOnResume = !(debugTools_ && debugTools_->IsDebugCamera());

	flow_->UpdatePaused(ctx);
	return true;
}

/// -------------------------------------------------------------
/// 通常時デバッグ更新
/// 
/// 凍結していない通常フレーム中だけ呼ばれる。
/// -------------------------------------------------------------
void GamePlayScene::UpdateDebug()
{
#ifdef USE_IMGUI
	if (!K4E::EditorModeController::GetInstance()->ShouldDrawDebugVisuals())
	{
		// Game Preview ModeではDebugカメラ操作を止め、実ゲーム確認を優先する。
		return;
	}
#endif // USE_IMGUI

	if (debugTools_)
	{
		debugTools_->UpdateDebugCamera(input_, world_.get());
	}
}

/// -------------------------------------------------------------
/// 通常ワールド更新
/// -------------------------------------------------------------
void GamePlayScene::UpdateWorld(float deltaTime)
{
	if (world_)
	{
		world_->Update(deltaTime);
		if (world_->HasStage1TutorialCompleted())
		{
			MarkStage1TutorialSeen();
		}
	}

	if (frustumCullingDebug_)
	{
		frustumCullingDebug_->Update(deltaTime);
	}

	if (effectController_)
	{
		effectController_->Update(deltaTime, world_.get());
	}
	if (bulletDecalManager_)
	{
		bulletDecalManager_->Update(deltaTime);
	}
}

/// -------------------------------------------------------------
/// クリア / ゲームオーバー判定
/// -------------------------------------------------------------
void GamePlayScene::CheckGameEnd()
{
	if (!world_ || !flow_)
	{
		return;
	}

	// プレイヤー死亡演出が終わったらゲームオーバーへ
	if (auto* player = world_->GetCharacters().GetPlayer())
	{
		if (player->IsDeathSequenceFinished())
		{
			flow_->EnterGameOver(input_);
			return;
		}
	}

	// ステージ固有の失敗条件
	if (world_->IsStageObjectiveFailed())
	{
		flow_->EnterGameOver(input_);
		return;
	}

	// ステージ固有のクリア条件
	if (world_->IsStageObjectiveCleared())
	{
		flow_->EnterGameClear(input_, nullptr);
		return;
	}
}

/// -------------------------------------------------------------
/// 3D描画
/// -------------------------------------------------------------
void GamePlayScene::Draw3DObjects()
{
	if (world_)
	{
		if (world_->IsBossIntroPresentationActive())
		{
			// ボス登場演出中に通常3D描画を止め、演出に必要な最小構成だけ描画する。
			world_->DrawBossIntro3D();
			return;
		}

		// 演出終了後は通常ゲーム描画へ戻す。
		world_->Draw3D(ShouldHideCharactersDuringIntro());
	}

	if (bulletDecalManager_)
	{
		bulletDecalManager_->Draw();
	}

	if (frustumCullingDebug_
#ifdef USE_IMGUI
		&& K4E::EditorModeController::GetInstance()->ShouldDrawDebugVisuals()
#endif // USE_IMGUI
		)
	{
		frustumCullingDebug_->DrawDebug();
	}
}

/// -------------------------------------------------------------
/// シャドウ描画
/// -------------------------------------------------------------
void GamePlayScene::DrawShadowObjects()
{
	if (world_)
	{
		if (world_->IsBossIntroPresentationActive())
		{
			world_->DrawBossIntroShadow();
			return;
		}

		world_->DrawShadow(ShouldHideCharactersDuringIntro());
	}
}

/// -------------------------------------------------------------
/// 2D描画
/// -------------------------------------------------------------
void GamePlayScene::Draw2DSprites()
{
	if (uiController_)
	{
		uiController_->Draw2DSprites(world_.get(), flow_.get(), fadeManager_.get(), ShouldHideGameplayUI());
	}
}

/// -------------------------------------------------------------
/// イントロ中のキャラクター非表示判定
/// 
/// Draw3D / DrawShadow / DrawHUD で同じ条件を使うため共通化
/// -------------------------------------------------------------
bool GamePlayScene::ShouldHideCharactersDuringIntro() const
{
	return (flow_ && flow_->IsIntro());
}

bool GamePlayScene::ShouldHideGameplayUI() const
{
	return ShouldHideCharactersDuringIntro() || (world_ && world_->IsBossIntroPresentationActive());
}

void GamePlayScene::RequestRetryWithFade()
{
	if (isRetryTransitionActive_)
	{
		return;
	}

	// リトライでは同じ練習を繰り返さず、すぐ再挑戦できるようチュートリアル完了扱いにする。
	MarkStage1TutorialSeen();
	RequestSkipStage1Tutorial();
	isRetryTransitionActive_ = true;
	isRetryRestartDone_ = false;

	StartRetryFadeOut();
}

/// -------------------------------------------------------------
/// 終了処理
/// -------------------------------------------------------------
void GamePlayScene::Finalize()
{
	RestoreCursorState();
	ReleaseGameplayObjects();

	if (fadeManager_)
	{
		fadeManager_->Finalize();
		fadeManager_.reset();
	}

	input_ = nullptr;
}

/// -------------------------------------------------------------
/// カーソル状態を通常へ戻す
/// -------------------------------------------------------------
void GamePlayScene::RestoreCursorState()
{
	if (input_)
	{
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}
}

/// -------------------------------------------------------------
/// 生成済みゲームプレイオブジェクト破棄
/// 
/// 依存が強いものから順に落とす意識で整理。
/// -------------------------------------------------------------
void GamePlayScene::ReleaseGameplayObjects()
{
	if (debugTools_)
	{
		debugTools_->Finalize();
		debugTools_.reset();
	}
	frustumCullingDebug_.reset();

	debugWindow_.reset();
	uiController_.reset();

	if (effectController_)
	{
		effectController_->Finalize(world_.get());
		effectController_.reset();
	}
	FinalizeBulletDecals();

	introDirector_.reset();

	if (flow_)
	{
		flow_->Finalize();
		flow_.reset();
	}

	if (world_)
	{
		world_->Finalize();
		world_.reset();
	}

	stageContext_.reset();
}

/// -------------------------------------------------------------
/// ImGui描画
/// -------------------------------------------------------------
void GamePlayScene::DrawImGui()
{
	if (debugWindow_)
	{
		debugWindow_->DrawImGui(world_.get(), debugTools_.get(), frustumCullingDebug_.get(), effectController_.get());
	}
	if (bulletDecalManager_)
	{
		bulletDecalManager_->DrawImGui();
	}
}

void GamePlayScene::StartLoad()
{
	loadStep_ = 0;
	isLoadReady_ = false;
}

void GamePlayScene::UpdateLoad()
{
	switch (loadStep_)
	{
	case 0:
		// フローは後続のWorld生成やイントロ開始判定から参照されるため最初に作る。
		flow_ = std::make_unique<GamePlayFlow>();
		flow_->Initialize();
		++loadStep_;
		break;

	case 1:
		// StageContextはステージ選択結果とLevelData由来のスポーン/Wave設定をまとめる。
		stageContext_ = std::make_unique<GamePlayStageContext>();
		stageContext_->InitializeFromRepository();
		++loadStep_;
		break;

	case 2:
		// World生成はステージ、衝突、キャラクターなど重い初期化を含むため単独ステップにする。
		world_ = std::make_unique<GamePlayWorld>();
		world_->Initialize(*stageContext_);
		InitializeBulletDecals();
		++loadStep_;
		break;

	case 3:
		introDirector_ = std::make_unique<GamePlayIntroDirector>();
		++loadStep_;
		break;

	case 4:
		// Debug系とHPポストエフェクトはWorldのPlayer参照が必要なのでWorld生成後に接続する。
		debugTools_ = std::make_unique<GamePlayDebugTools>();
		debugTools_->Initialize();
		frustumCullingDebug_ = std::make_unique<FrustumCullingDebugController>();
		frustumCullingDebug_->Initialize();

		uiController_ = std::make_unique<GamePlayUIController>();
		debugWindow_ = std::make_unique<GamePlayDebugWindow>();
		InitializeEffectController();
		++loadStep_;
		break;

	case 5:
		// 初回ロードはステージ側にイントロ点があればイントロありで開始する。
		SetupNewGame(false);
		isLoadReady_ = true;
		++loadStep_;
		break;

	default:
		break;
	}
}

void GamePlayScene::StartUnload()
{
	unloadStep_ = 0;
	isUnloadReady_ = false;
}

void GamePlayScene::UpdateUnload()
{
	switch (unloadStep_)
	{
	case 0:
		// シーン遷移中にカーソルがロックされたまま残らないよう、最初に入力状態を戻す。
		RestoreCursorState();
		++unloadStep_;
		break;

	case 1:
		// Debug系はWorld参照を持つため、World本体より先に破棄して古い参照を残さない。
		if (debugTools_)
		{
			debugTools_->Finalize();
			debugTools_.reset();
		}
		frustumCullingDebug_.reset();
		debugWindow_.reset();
		uiController_.reset();
		if (effectController_)
		{
			effectController_->Finalize(world_.get());
			effectController_.reset();
		}
		FinalizeBulletDecals();
		++unloadStep_;
		break;

	case 2:
		// UI/イントロ系はWorldを直接所有しないが、更新順上はWorldより先に止めておく。
		introDirector_.reset();

		if (flow_)
		{
			flow_->Finalize();
			flow_.reset();
		}
		++unloadStep_;
		break;

	case 3:
		// World解放は衝突、弾、ステージ、キャラクターをまとめて落とすため単独フレームにする。
		if (world_)
		{
			world_->Finalize();
			world_.reset();
		}
		++unloadStep_;
		break;

	case 4:
		// StageContextはWorldの初期化元なので、World破棄後に最後に解放する。
		stageContext_.reset();
		++unloadStep_;
		break;

	case 5:
		// SceneManager側の遷移フェードへ制御が移るため、Scene内Fadeはここで解放する。
		if (fadeManager_)
		{
			fadeManager_->Finalize();
			fadeManager_.reset();
		}

		input_ = nullptr;

		isUnloadReady_ = true;
		++unloadStep_;
		break;

	default:
		break;
	}
}

bool GamePlayScene::IsReadyToStartUncover() const
{
	return isLoadReady_;
}

bool GamePlayScene::IsReadyToSwapOut() const
{
	return isUnloadReady_;
}

/// -------------------------------------------------------------
/// リスタート
/// 
/// まず完全に終了してから初期化し直す。
/// 現状はこの形が最も分かりやすい。
/// -------------------------------------------------------------
void GamePlayScene::RestartGame(bool skipIntro)
{
	// カーソルをゲームプレイ向けに戻す
	if (input_)
	{
		input_->SetLockCursor(true);
		input_->SetCursorVisible(false);
	}

	// フェードは残したまま、ゲームプレイ構成だけ再生成
	ReleaseGameplayObjects();
	InitializeGameplayObjects(!ShouldStartStage1Tutorial());
	stage1TutorialSkipRequested_ = false;
	SetupNewGame(skipIntro);
}

bool GamePlayScene::ShouldStartStage1Tutorial() const
{
	return !stage1TutorialSkipRequested_ && !tutorialSeen_;
}

void GamePlayScene::MarkStage1TutorialSeen()
{
	// 完了済みフラグは同じ起動中の2回目以降にスキップUIを足せる土台として保持する。
	tutorialSeen_ = true;
	tutorialSkipUnlocked_ = true;
}

void GamePlayScene::RequestSkipStage1Tutorial()
{
	if (tutorialSkipUnlocked_)
	{
		stage1TutorialSkipRequested_ = true;
	}
}

void GamePlayScene::StartRetryFadeOut()
{
	if (fadeManager_)
	{
		fadeManager_->StartCover();
	}
}

bool GamePlayScene::IsRetryFadeOutFinished() const
{
	return fadeManager_ && fadeManager_->IsFullyCovered();
}

void GamePlayScene::StartRetryFadeIn()
{
	if (fadeManager_)
	{
		fadeManager_->StartCrack();
	}
}

bool GamePlayScene::IsRetryFadeInFinished() const
{
	return fadeManager_ && fadeManager_->IsDropDone();
}
void GamePlayScene::CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& outObjects)
{
	const auto setCommonInspectorHint = [](Ken4lowEngine::EditorObjectInfo& object, const char* hint)
		{
			object.inspectorHint = hint;
			object.transformUnavailableReason = "Transform editing is not available for this GamePlayScene item.";
		};
	const auto addManagerObject = [&outObjects, this, &setCommonInspectorHint](uint64_t id, const char* displayName, const char* typeName, Ken4lowEngine::EditorInspectorType inspectorType)
		{
			// Manager系はTransformを触らず、Detailsへ落ちない簡易情報だけを安定IDで公開する。
			Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "GamePlayScene" };
			object.inspectorType = inspectorType;
			setCommonInspectorHint(object, "GamePlayScene Manager Info");
			object.drawInspector = [this, displayName]()
				{
#ifdef USE_IMGUI
					ImGui::TextUnformatted("GamePlayScene runtime summary");
					ImGui::Separator();
					if (std::string_view(displayName) == "Enemy Manager")
					{
						const int enemyCount = world_ ? world_->GetCharacters().GetEnemyCount() : -1;
						if (enemyCount >= 0)
						{
							ImGui::Text("Active Enemy Count: %d", enemyCount);
						}
						else
						{
							ImGui::TextUnformatted("Active Enemy Count: N/A");
						}
						ImGui::TextUnformatted("Total Enemy Count: N/A");
						ImGui::TextUnformatted("Use Enemy Debug window for detailed tuning.");
					}
					else if (std::string_view(displayName) == "Bullet Manager")
					{
						auto* bulletManager = world_ ? world_->GetBulletManager() : nullptr;
						if (bulletManager)
						{
							ImGui::Text("Active Bullet Count: %zu", bulletManager->GetCount());
						}
						else
						{
							ImGui::TextUnformatted("Active Bullet Count: N/A");
						}
						ImGui::TextUnformatted("Player Bullet Count: N/A");
						ImGui::TextUnformatted("Enemy Bullet Count: N/A");
						ImGui::TextUnformatted("Use Weapon Debug / Bullet Debug for detailed tuning.");
					}
					else if (std::string_view(displayName) == "Wave Manager")
					{
						auto* waveManager = world_ ? world_->GetWaveManager() : nullptr;
						if (waveManager)
						{
							ImGui::Text("Current Wave: %d / %d", waveManager->GetCurrentWaveNumber(), waveManager->GetTotalWaveCount());
							const char* waveState = waveManager->IsAllWavesCleared() ? "All Cleared" : (waveManager->IsWaveInProgress() ? "In Progress" : (waveManager->IsWaitingNextWave() ? "Waiting Next Wave" : (waveManager->HasStarted() ? "Started" : "Not Started")));
							ImGui::Text("Wave State: %s", waveState);
							ImGui::Text("Remaining Enemies: %d", world_->GetCharacters().GetEnemyCount());
							ImGui::Text("Is Wave Active: %s", waveManager->IsWaveInProgress() ? "true" : "false");
						}
						else
						{
							ImGui::TextUnformatted("Current Wave: N/A");
							ImGui::TextUnformatted("Wave State: N/A");
							ImGui::TextUnformatted("Remaining Enemies: N/A");
							ImGui::TextUnformatted("Is Wave Active: N/A");
						}
					}
					else if (std::string_view(displayName) == "HUD")
					{
						auto* hud = world_ ? world_->GetHUDManager() : nullptr;
						ImGui::Text("Visible: %s", hud ? "Available" : "N/A");
						ImGui::Text("HP UI: %s", (hud && hud->GetHPWidget()) ? (hud->GetHPWidget()->IsVisible() ? "Visible" : "Hidden") : "N/A");
						ImGui::Text("Ammo UI: %s", (hud && hud->GetWeaponSlot()) ? "Available" : "N/A");
						ImGui::Text("Reticle: %s", (hud && hud->GetCrosshair()) ? (hud->GetCrosshair()->IsVisible() ? "Visible" : "Hidden") : "N/A");
						ImGui::Text("Damage Indicator: %s", hud ? "Managed by HUD" : "N/A");
						ImGui::TextUnformatted("Use HUD Debug window for detailed tuning.");
					}
					else if (std::string_view(displayName) == "Collision Manager")
					{
						auto* collisionManager = world_ ? world_->GetCollisionManager() : nullptr;
						if (collisionManager)
						{
							ImGui::Text("Collider Count: %zu", collisionManager->GetColliderCount());
						}
						else
						{
							ImGui::TextUnformatted("Collider Count: N/A");
						}
						ImGui::TextUnformatted("Collision Pair Count: N/A");
						ImGui::TextUnformatted("Debug Draw Enabled: N/A");
						ImGui::TextUnformatted("Use Collision Debug window for detailed tuning.");
					}
					else
					{
						ImGui::TextUnformatted("No editable Transform is available for this item.");
						ImGui::TextUnformatted("Use a dedicated Debug window for detailed editing when available.");
					}
#endif
				};
			outObjects.push_back(std::move(object));
		};
	const auto addCameraObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
		{
			// GamePlaySceneのMain CameraはCameraManagerから毎フレーム取り直して安全に編集する。
			outObjects.push_back(Ken4lowEngine::MakeCameraEditorObject(id, displayName, typeName, "GamePlayScene", K4E::CameraManager::GetInstance()->GetMainCamera()));
		};
	const auto addLightObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
		{
			// LightManagerの先頭ライトを安全なindex指定でDetails編集へ公開する。
			Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "GamePlayScene" };
			object.inspectorType = Ken4lowEngine::EditorInspectorType::PunctualLights;
			outObjects.push_back(std::move(object));
		};
	const auto addPlayerObject = [&outObjects, this, &setCommonInspectorHint](uint64_t id, const char* displayName, const char* typeName)
		{
			// Playerは編集より状態確認を優先し、古い選択で落ちない読み取り専用Inspectorにする。
			Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "GamePlayScene" };
			object.inspectorType = Ken4lowEngine::EditorInspectorType::PlayerInfo;
			setCommonInspectorHint(object, "GamePlayScene Player Inspector");
			object.drawInspector = [this]()
				{
#ifdef USE_IMGUI
					Player* player = world_ ? world_->GetCharacters().GetPlayer() : nullptr;
					ImGui::TextUnformatted("Player Inspector");
					ImGui::Separator();
					if (!player)
					{
						ImGui::TextUnformatted("HP: N/A");
						ImGui::TextUnformatted("Position: N/A");
						ImGui::TextUnformatted("Rotation: N/A");
						ImGui::TextUnformatted("Scale: N/A");
						ImGui::TextUnformatted("Alive/Dead: N/A");
						ImGui::Text("Input Enabled: %s", input_ ? (input_->IsGameInputEnabled() ? "Enabled" : "Disabled") : "N/A");
						ImGui::TextUnformatted("Player transform editing is not implemented yet.");
						return;
					}
					ImGui::Text("HP: %.1f / %.1f", player->GetHP(), player->GetMaxHP());
					auto* bodyTransform = player->GetWorldTransform();
					if (bodyTransform)
					{
						ImGui::Text("Position: %.2f, %.2f, %.2f", bodyTransform->translate_.x, bodyTransform->translate_.y, bodyTransform->translate_.z);
						ImGui::Text("Rotation: %.2f, %.2f, %.2f", bodyTransform->rotate_.x, bodyTransform->rotate_.y, bodyTransform->rotate_.z);
						ImGui::Text("Scale: %.2f, %.2f, %.2f", bodyTransform->scale_.x, bodyTransform->scale_.y, bodyTransform->scale_.z);
					}
					else
					{
						ImGui::TextUnformatted("Position: N/A");
						ImGui::TextUnformatted("Rotation: N/A");
						ImGui::TextUnformatted("Scale: N/A");
					}
					ImGui::Text("Alive/Dead: %s", (player->GetHP() <= 0.0f || player->IsDeathActive()) ? "Dead" : "Alive");
					ImGui::Text("Input Enabled: %s", input_ ? (input_->IsGameInputEnabled() ? "Enabled" : "Disabled") : "N/A");
					ImGui::TextUnformatted("Player transform editing is not implemented yet.");
#endif
				};
			outObjects.push_back(std::move(object));
		};
	const auto addStageObject = [&outObjects, this, &setCommonInspectorHint](uint64_t id, const char* displayName, const char* typeName)
		{
			// StageはリポジトリとLevelDataから安全に取れる概要だけをDetailsへ表示する。
			Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "GamePlayScene" };
			object.inspectorType = Ken4lowEngine::EditorInspectorType::StageInfo;
			setCommonInspectorHint(object, "GamePlayScene Stage Inspector");
			object.drawInspector = [this]()
				{
#ifdef USE_IMGUI
					auto* stage = world_ ? world_->GetStage() : nullptr;
					const int stageIndex = stageContext_ ? stageContext_->GetCurrentStageIndex() : -1;
					const auto& stages = StageRepository::GetInstance().GetStages();
					const StageInfo* stageInfo = (stageIndex >= 0 && static_cast<size_t>(stageIndex) < stages.size()) ? &stages[stageIndex] : nullptr;
					ImGui::TextUnformatted("Stage Inspector");
					ImGui::Separator();
					if (stageInfo)
					{
						ImGui::Text("Stage ID: %u", stageInfo->id);
						ImGui::Text("Stage Name: %s", stageInfo->name.c_str());
						ImGui::Text("Stage Type: %s", stageInfo->category.c_str());
					}
					else
					{
						ImGui::TextUnformatted("Stage ID: N/A");
						ImGui::TextUnformatted("Stage Name: N/A");
						ImGui::TextUnformatted("Stage Type: N/A");
					}
					const auto* levelData = stage ? stage->GetLevelData() : nullptr;
					if (levelData)
					{
						ImGui::Text("Object Count: %zu", levelData->objects.size());
					}
					else
					{
						ImGui::TextUnformatted("Object Count: N/A");
					}
					if (stage)
					{
						ImGui::Text("Collision Count: %zu", stage->GetWorldColliders().size());
					}
					else
					{
						ImGui::TextUnformatted("Collision Count: N/A");
					}
#endif
				};
			outObjects.push_back(std::move(object));
		};

	// OutlinerはPlay/Edit中の生成状態に依存しすぎないよう、主要サブシステムを安定IDで列挙する。
	addManagerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.Root"), "GamePlay Root", "Scene Root", Ken4lowEngine::EditorInspectorType::ManagerInfo);
	addPlayerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.Player"), "Player", "Player");
	addCameraObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.MainCamera"), "Main Camera", "Camera");
	addLightObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.PunctualLights"), "Punctual Lights", "Light Manager / Punctual Lights");
	addManagerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.EnemyManager"), "Enemy Manager", "Enemy Manager", Ken4lowEngine::EditorInspectorType::EnemyManagerInfo);
	addManagerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.BulletManager"), "Bullet Manager", "Bullet Manager", Ken4lowEngine::EditorInspectorType::BulletManagerInfo);
	addManagerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.WaveManager"), "Wave Manager", "Wave Manager", Ken4lowEngine::EditorInspectorType::WaveManagerInfo);
	addStageObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.Stage"), "Stage", "Stage");
	addManagerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.HUD"), "HUD", "HUD", Ken4lowEngine::EditorInspectorType::HudInfo);
	addManagerObject(Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.CollisionManager"), "Collision Manager", "Collision Manager", Ken4lowEngine::EditorInspectorType::CollisionManagerInfo);
	{
		Ken4lowEngine::EditorObjectInfo fadeObject{ Ken4lowEngine::MakeStableEditorObjectId("GamePlayScene.FadeManager"), "FadeManager", "Fade Manager", "GamePlayScene" };
		fadeObject.inspectorType = Ken4lowEngine::EditorInspectorType::FadeManager;
		fadeObject.drawInspector = [this]()
			{
				if (fadeManager_)
				{
					fadeManager_->DrawInspectorContent();
				}
			};
		outObjects.push_back(std::move(fadeObject));
	}
}
