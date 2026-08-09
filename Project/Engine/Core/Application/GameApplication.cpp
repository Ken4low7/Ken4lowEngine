#define NOMINMAX
#include "GameApplication.h"
#include "SceneFactory.h"
#include "ParameterManager.h"
#include "ParticleManager.h"
#include <Wireframe.h>
#include <DirectXCommon.h>
#include "Object3DCommon.h"
#include "PostEffectManager.h"
#include <GpuParticleManager.h>
#include <SceneManager.h>
#include <FadeManager.h>
#include <Input.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#include "Editor/EditorGpuPickingManager.h"
#include "Editor/EditorSelectionOutlineManager.h"
#include "Editor/EditorShell.h"
#include "Editor/EditorWindowManager.h"
#include "Editor/EditorModeController.h"
#endif // USE_IMGUI
#include "JsonAssets/JsonEditorWindow.h"
#include <DisplaySettings.h>
#include <WinApp.h>

namespace Ken4lowEngine
{
	GameApplication::GameApplication() = default;

	GameApplication::~GameApplication() = default;

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　初期化処理
	/// -------------------------------------------------------------
	void GameApplication::Initialize()
	{
		// Framework 側でウィンドウ、DirectX、共通描画マネージャを先に初期化する。
		Framework::Initialize();

		// RenderPipelineControllerは既存描画関数を順番に呼ぶだけの薄い入口として初期化する。
		renderPipelineController_.Initialize(dxCommon_);

#ifdef USE_IMGUI
		// DebugビルドはEditor Mode ON、Release相当ではGame Preview Modeとして初期化する。
		EditorModeController::GetInstance()->Initialize();
		EditorGpuPickingManager::GetInstance()->Initialize(); // Descriptor Manager初期化後にR32_UINT ID Bufferを生成する。
		EditorSelectionOutlineManager::GetInstance()->Initialize(); // 選択Object専用のMask/Outline Textureを生成する。
#endif // USE_IMGUI

		/// ---------- 入力の初期化 ---------- ///
		// WinApp のウィンドウハンドルを使って、キーボード・マウス・ゲームパッド入力を受け取れる状態にする。
		Input::GetInstance()->Initialize(winApp_);

		// JSON で保存されたグローバルパラメータを読み込み、起動直後から調整値を反映できるようにする。
		ParameterManager::GetInstance()->LoadFiles();

		// JSON アセット確認用のエディタウィンドウを初期化する。
		JsonEditorWindow::GetInstance()->Initialize();

		// SceneManagerを生成し、GameApplicationが所有権を持つ。
		sceneManager_ = std::make_unique<SceneManager>();

		// ゲーム固有の岩ブロック遷移演出をEngine側のSceneManagerへ注入する。
		sceneManager_->SetSceneTransition(std::make_unique<FadeManager>());
		sceneManager_->Initialize();

#ifdef USE_IMGUI
		// EditorWindowManagerは所有せず、現在のSceneManagerへの参照だけを保持する。
		EditorWindowManager::GetInstance()->SetSceneManager(sceneManager_.get());
#endif // USE_IMGUI

		// 文字列のシーン名から実際の Scene インスタンスを作れるよう、Factory を SceneManager へ登録する。
		auto sceneFactory = std::make_unique<SceneFactory>();
		sceneManager_->SetAbstractSceneFactory(std::move(sceneFactory));

#ifdef _DEBUG
		// Debugビルドでは最初からDebugSceneを起動して、ゲームプレイ中もすぐ切り替えられるようにする。
		const std::string startSceneName = "DebugScene";
#else
		// Releaseビルドでは最初のシーンをTitleSceneにする。
		const std::string startSceneName = "TitleScene";
#endif
		// 起動直後に表示するシーンを SceneManager へ依頼する。
		sceneManager_->ChangeScene(startSceneName);
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　更新処理
	/// -------------------------------------------------------------
	void GameApplication::Update()
	{
		// フレーム開始時刻を記録し、DeltaTime や各フェーズ計測の基準を作る。
		GameTimer::GetInstance()->BeginFrame();

		// Update フェーズの処理時間を計測する。
		GameTimer::GetInstance()->BeginUpdate();

		// 前フレームとの差分を取れるよう、ゲーム処理より前に入力状態を更新する。
		Input::GetInstance()->Update();

#ifdef USE_IMGUI
		// F1でEditor Mode / Game Preview Modeを切り替え、Preview中はゲーム入力を優先する。
		EditorModeController::GetInstance()->Update(Input::GetInstance());
		// FrameworkがCameraやParticleのGPU処理を積む前に、予約されたPIE Worldの生成・破棄を完了する。
		sceneManager_->ProcessEditorPlayRequests();
#endif // USE_IMGUI

		// FPSカメラを使っていない場面でも、メインカメラの行列を最新状態にしておく。
		if (defaultCamera_)
		{
			defaultCamera_->Update();
		}

		// CameraManager や Particle など、ゲーム全体で共通する更新を実行する。
		Framework::Update();

		// 現在シーン固有の Update を呼び出す。
		sceneManager_->Update();

		// ポストエフェクトや JSON エディタなど、シーン外の補助機能を更新する。
		PostEffectManager::GetInstance()->Update();
		JsonEditorWindow::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime());

		// Update フェーズ終了後にフレーム時間を確定させる。
		GameTimer::GetInstance()->EndFrame();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　描画処理
	/// -------------------------------------------------------------
	void GameApplication::Draw()
	{
		// Draw フェーズの処理時間を計測する。
		GameTimer::GetInstance()->BeginDraw();

		RenderPipelineController::FrameCallbacks callbacks{};
		callbacks.prepareShadowPass = [this]()
			{
				sceneManager_->PrepareShadowPass(); // LightComponentの最新値をShadowSystemのCaster選択より先に反映する。
			};
		callbacks.drawShadowObjects = [this]()
			{
				// 影を落とす3Dオブジェクトだけを描画し、ShadowMapへ深度を書き込む既存処理を呼ぶ。
				sceneManager_->DrawShadowObjects();
			};
		callbacks.drawGameWorldToSceneTarget = [this]()
			{
				DrawGameWorldToSceneTarget();
			};
		callbacks.renderPostEffectToGameRenderTarget = []()
			{
				// Editor ModeではBackBufferではなくMain Viewport用GameRenderTargetへPostEffect結果を集約する。
				PostEffectManager::GetInstance()->RenderPostEffect();
			};
		callbacks.beginGameRenderTargetOverlay = []()
			{
				PostEffectManager::GetInstance()->BeginGameRenderTargetOverlay();
			};
		callbacks.drawScene2DOverlay = [this]()
			{
				DrawCurrentScene2DOverlay();
			};
		callbacks.endGameRenderTargetOverlay = []()
			{
				PostEffectManager::GetInstance()->EndGameRenderTargetOverlay();
			};
		callbacks.applyPostEffectToBackBuffer = [this]()
			{
				ApplyPostEffectToBackBuffer();
			};
		callbacks.rebindBackBufferForGameOverlay = [this]()
			{
				dxCommon_->RebindBackBufferForGameOverlay();
			};
		callbacks.drawGameUIToBackBuffer = [this]()
			{
				DrawGameUIToBackBuffer();
			};

		bool editorModeEnabled = false;
#ifdef USE_IMGUI
		callbacks.drawImGuiOverlay = []()
			{
				ImGuiManager::GetInstance()->Draw();
			};
		editorModeEnabled = EditorModeController::GetInstance()->ShouldDrawEditorUi();
		if (editorModeEnabled)
		{
			callbacks.renderEditorSelectionOutline = [this]()
				{
					EditorSelectionOutlineManager::GetInstance()->Render(sceneManager_->GetCurrentScene());
				};
			callbacks.buildEditorUi = [this]()
				{
					/// ---------- ImGuiフレーム開始 ---------- ///
					ImGuiManager::GetInstance()->BeginFrame();

					// UE5風エディタUI土台を既存のシーン別ImGuiの前に描画する
					auto* editorWindows = EditorWindowManager::GetInstance();
					editorWindows->Draw();
					auto& editorWindowState = editorWindows->GetWindowState();
					JsonEditorWindow::GetInstance()->Draw(&editorWindowState.showJsonAssetManager);

					// WindowメニューのDisplay表示フラグをWinApp側の×ボタン状態と共有する
					winApp_->DrawDisplaySettingsImGui(&editorWindowState.showDisplay);

					// WindowメニューのParameters表示フラグをParameterManager側の×ボタン状態と共有する
					ParameterManager::GetInstance()->Update(&editorWindowState.showParameters);

					//defaultCamera_->DrawImGui();

					// ImGuiを描画
					Object3DCommon::GetInstance()->DrawImGui();

					// シーンのImGuiの描画処理
					sceneManager_->DrawImGui();

					// ParticleManagerのImGuiの描画処理
					ParticleManager::GetInstance()->DrawImGui();

					// WindowメニューのPost Effect Settings表示フラグをPostEffectManager側の×ボタン状態と共有する
					PostEffectManager::GetInstance()->ImGuiRender(&editorWindowState.showPostEffectSettings);

					// 他のEditorパネルより後に描き、ビューポートツールバーを最前面へ配置する。
					EditorShell::GetInstance()->DrawViewportOverlay();

					/// ---------- ImGuiフレーム終了 ---------- ///
					ImGuiManager::GetInstance()->EndFrame();

					// Debug/Editor時もゲーム内部解像度は固定し、Main Viewport側の表示だけを拡縮する。
					(void)EditorWindowManager::GetInstance()->GetMainViewportSize();
				};
			callbacks.executeEditorPickingPass = [this]()
				{
					const EditorGpuPickingManager::ExecuteResult result =
						EditorGpuPickingManager::GetInstance()->Execute(sceneManager_->GetCurrentScene());
					if (result.executed && !result.message.empty())
					{
						EditorWindowManager::GetInstance()->AddOutputLog(
							result.hit ? EditorLogLevel::Info : EditorLogLevel::Info,
							result.message);
					}
				};
		}
#endif // USE_IMGUI

		// GameApplication::Drawは既存互換のFacadeとして残し、描画順序の実行だけをControllerへ委譲する。
		renderPipelineController_.ExecuteFrame(editorModeEnabled, callbacks);

		// Draw計測終了
		// ※ EndDraw() の中に Present が含まれている可能性が高いので、
		//   その手前までを Draw として区切る
		GameTimer::GetInstance()->EndDraw();

		// Present計測開始
		GameTimer::GetInstance()->BeginPresent();

		//--------------------------------------------
		// 7. 描画終了
		//--------------------------------------------
		dxCommon_->EndDraw();

		// Present計測終了
		GameTimer::GetInstance()->EndPresent();

		// フレーム終了
		GameTimer::GetInstance()->EndFrame();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　終了処理
	/// -------------------------------------------------------------
	void GameApplication::Finalize()
	{
#ifdef USE_IMGUI
		// SceneManager破棄後にEditorが古い参照へアクセスしないよう先に解除する。
		EditorWindowManager::GetInstance()->SetSceneManager(nullptr);
		EditorSelectionOutlineManager::GetInstance()->Finalize(); // Descriptor Manager破棄前に輪郭用GPU Resourceを解放する。
		EditorGpuPickingManager::GetInstance()->Finalize(); // Descriptor Manager破棄前にPicking用GPU Resourceを解放する。
#endif // USE_IMGUI

		{
			// 所有権をローカルへ移し、描画基盤を終了する前にスコープ終了で自動破棄する。
			auto sceneManager = std::move(sceneManager_);
			if (sceneManager)
			{
				sceneManager->Finalize();
			}
		}

		Framework::Finalize();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　ゲーム本編3D描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene3DPass()
	{
		// Scene 3D -> Debug Wireframe -> GPU Particle -> CPU Particle の順を Debug / Release で固定する。
		Object3DCommon::GetInstance()->BeginObject3DPass();

		// 現在シーンが持つ通常の3Dモデルを最初に描画する。
		sceneManager_->Draw3DObjects();

		// デバッグ表示、GPUパーティクル、CPUパーティクルをモデル描画後に重ねる。
		Wireframe::GetInstance()->Draw();
		GpuParticleManager::GetInstance()->Draw();
		ParticleManager::GetInstance()->Draw();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　HUD/UI/Sprite描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene2DOverlay()
	{
		// GamePlayScene::Draw2DSprites() 内で HUDManager / Reticle / Ammo / HP / Fade まで描画する。
		sceneManager_->Draw2DSprites();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　SceneRenderTargetへのゲーム本編描画処理
	/// -------------------------------------------------------------
	void GameApplication::DrawGameWorldToSceneTarget()
	{
		// Debug / Release とも SceneRenderTarget へ 3D World + Particle を描画し、PostEffect 入力を必ず作る。
		PostEffectManager::GetInstance()->BeginDraw();
		DrawCurrentScene3DPass();
		PostEffectManager::GetInstance()->EndDraw();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　BackBufferへのポストエフェクト反映処理
	/// -------------------------------------------------------------
	void GameApplication::ApplyPostEffectToBackBuffer()
	{
		// SceneRenderTarget を入力にした PostEffect 結果を BackBuffer へ出力する。
		PostEffectManager::GetInstance()->RenderPostEffectToBackBuffer();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　BackBufferへのUI描画処理
	/// -------------------------------------------------------------
	void GameApplication::DrawGameUIToBackBuffer()
	{
		// HUD / UI / Sprite / Font は PostEffect 後の BackBuffer へ直接描画する。
		DrawCurrentScene2DOverlay();
	}

} // namespace Ken4lowEngine
