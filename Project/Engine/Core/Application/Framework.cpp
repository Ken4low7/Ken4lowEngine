#include "Framework.h"
#include <Windows.h>
#include <WinApp.h>
#include <DirectXCommon.h>
#include <DSVManager.h>
#include <RTVManager.h>
#include <SRVManager.h>
#include <UAVManager.h>
#include <TextureManager.h>
#include <ParticleManager.h>
#include <SpriteManager.h>
#include <Object3DCommon.h>
#include <ModelManager.h>
#include <CameraManager.h>
#include <DebugCamera.h>
#include <Wireframe.h>
#include <AnimationPipelineBuilder.h>
#include <SkyBoxManager.h>
#include <PostEffectManager.h>
#include <BlendStateFactory.h>
#include "GpuParticleManager.h"
#include <GameTimer.h>
#include <ResolutionManager.h>

#ifdef USE_IMGUI
#include <Editor/EditorWindowManager.h>
#include <ImGuiManager.h>
#endif // USE_IMGUI

#ifdef _DEBUG
#include <D3DResourceLeakChecker.h>
#endif // _DEBUG

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　		ゲーム全体の実行処理
	/// -------------------------------------------------------------
	void Framework::Run()
	{
		// 初期化処理
		Initialize();

		// ゲームループ
		while (!winApp_->ProcessMessage())// 終了リクエストが来たら抜ける
		{
			// Alt+Enter の入力要求を検知し、現在の表示モードに応じて次の表示設定を組み立てる。
			if (winApp_->ConsumeToggleFullscreen())
			{
				DisplaySettings cur = winApp_->GetCurrentDisplaySettings();
				DisplaySettings next = cur;

				if (cur.mode == WindowMode::Windowed)
				{
					// Windowed→Borderless に切り替える前に、戻し先となるウィンドウ設定を保存しておく。
					winApp_->RememberWindowedSettings(cur);
					next.mode = WindowMode::BorderlessFullscreen;
				}
				else
				{
					// Borderless→Windowed では、最後に使っていたウィンドウサイズと位置へ戻す。
					next = winApp_->GetLastWindowedSettingsOrDefault();
					next.mode = WindowMode::Windowed;
				}

				winApp_->RequestDisplaySettings(next);
			}

			// WinApp 側で予約された表示設定を受け取るための一時データ。
			DisplaySettings ds;

			// 表示設定の変更要求がある場合のみ、実際にウィンドウへ適用する。
			if (winApp_->ConsumeDisplaySettings(ds))
			{
				winApp_->ApplyDisplaySettings(ds);
			}

			// OS ウィンドウのリサイズ要求を検知し、描画バッファとUI基準解像度を同期する。
			uint32_t newWidth = 0;
			uint32_t newHeight = 0;

			if (winApp_->ConsumeResize(newWidth, newHeight))
			{
				dxCommon_->Resize(newWidth, newHeight);
				// 現在解像度を集約し、UI座標変換とCameraのProjectionを同じ基準へ揃える。
				ResolutionManager::GetInstance()->SetScreenSize(static_cast<float>(newWidth), static_cast<float>(newHeight));

				if (defaultCamera_)
				{
					defaultCamera_->SetAspectRatio(ResolutionManager::GetInstance()->GetAspectRatio());
				}
			}

			// 入力・シーン・各種マネージャなどのゲーム状態を1フレーム進める。
			Update();

			// 更新済みの状態をもとに、3D / 2D / UI を描画する。
			Draw();
		}

		// ゲームの終了
		Finalize();
	}


	/// -------------------------------------------------------------
	///				　　	 　ゲーム全体の初期化処理
	/// -------------------------------------------------------------
	void Framework::Initialize()
	{
#pragma region ---------- ウィンドウアプリケーションの初期化処理 ----------
		// ウィンドウアプリケーションの生成
		winApp_ = WinApp::GetInstance();

		DisplaySettings ds{};
#ifdef USE_IMGUI
		ds.mode = WindowMode::Windowed; // DebugのImGui EditorではOSウィンドウ内にMain Viewportを表示する
#else
		ds.mode = WindowMode::BorderlessFullscreen; // Releaseでは従来通りゲーム全体をボーダーレス表示する
#endif // USE_IMGUI
		ds.monitorIndex = 0;

		winApp_->CreateMainWindow(ds);
		// 起動時の実クライアントサイズを記録し、解像度非依存のUI/入力変換へ使う。
		ResolutionManager::GetInstance()->SetScreenSize(
			static_cast<float>(winApp_->GetClientWidth()),
			static_cast<float>(winApp_->GetClientHeight())
		);
#pragma endregion ---------------------------------------------------------


#pragma region ---------- 基盤システムの初期化処理 ----------
		// DirectX共通クラスの生成
		dxCommon_ = DirectXCommon::GetInstance();
		dxCommon_->Initialize(winApp_, winApp_->GetClientWidth(), winApp_->GetClientHeight());

#ifdef USE_IMGUI
		// ImGuiManagerの初期化
		ImGuiManager::GetInstance()->Initialize(winApp_, dxCommon_);
#endif // USE_IMGUI

		// UAVマネージャーの初期化
		UAVManager::GetInstance()->Initialize(dxCommon_);

		// テクスチャマネージャーの初期化
		TextureManager::GetInstance()->Initialize(dxCommon_);

		// ブレンドステートファクトリの初期化
		BlendStateFactory::GetInstance()->Initialize();

		// スプライトマネージャの初期化
		SpriteManager::GetInstance()->Initialize(dxCommon_);

		// Object3DCommonの初期化
		Object3DCommon::GetInstance()->Initialize(dxCommon_);

		// アニメーションパイプラインビルダーの初期化
		AnimationPipelineBuilder::GetInstance()->Initialize(dxCommon_);

		// デバッグカメラの初期化
		DebugCamera::GetInstance()->Initialize();

		// デフォルトカメラの生成と初期化
		defaultCamera_ = std::make_unique<Camera>();
		defaultCamera_->SetRotate({ 0.3f, 0.0f, 0.0f });
		defaultCamera_->SetTranslate({ 0.0f, 10.0f, -20.0f });
		// CameraのProjectionも現在解像度のAspectへ合わせ、レイ判定と見た目のズレを防ぐ。
		defaultCamera_->SetAspectRatio(ResolutionManager::GetInstance()->GetAspectRatio());
		defaultCamera_->Update();

		// カメラの司令塔の初期化
		CameraManager::GetInstance()->Initialize();
		CameraManager::GetInstance()->SetMainCamera(defaultCamera_.get());
		CameraManager::GetInstance()->SetUseDebugCamera(false);

		// ワイヤーフレームのカメラ設定
		Wireframe::GetInstance()->SetCamera(defaultCamera_.get());

		// ワイヤーフレームの初期化
		Wireframe::GetInstance()->Initialize(dxCommon_);

		// ParticleManagerの初期化
		ParticleManager::GetInstance()->Initialize(dxCommon_, defaultCamera_.get());

		// スカイボックスの初期化
		SkyBoxManager::GetInstance()->Initialize(dxCommon_);

		// ポストエフェクトの初期化
		PostEffectManager::GetInstance()->Initialize(dxCommon_);

		// GPUパーティクルマネージャーの初期化
		GpuParticleManager::GetInstance()->Initialize(defaultCamera_.get());

#pragma endregion -------------------------------------------
	}


	/// -------------------------------------------------------------
	///				　		ゲーム全体の更新処理
	/// -------------------------------------------------------------
	void Framework::Update()
	{
		// カメラ司令塔の更新
		CameraManager::GetInstance()->Update();

		// ワイヤーフレームの更新処理
		Wireframe::GetInstance()->Update();

		// ParticleManagerの更新処理
		ParticleManager::GetInstance()->Update();

		// Gpuパーティクルマネージャーの更新処理
		GpuParticleManager::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime());
	}


	/// -------------------------------------------------------------
	///				　		ゲーム全体の終了処理
	/// -------------------------------------------------------------
	void Framework::Finalize()
	{
#ifdef USE_IMGUI
		// TextureManager/SRVManager/DirectXCommonより先にエディタ用プレビューキャッシュを解放する。
		EditorWindowManager::GetInstance()->FinalizeEditorServices();
#endif // USE_IMGUI

		// GPUパーティクルマネージャーの終了処理
		GpuParticleManager::GetInstance()->Finalize();

		// ポストエフェクトの終了処理
		PostEffectManager::GetInstance()->Finalize();

		// スカイボックスの終了処理
		SkyBoxManager::GetInstance()->Finalize();

		// ParticleManagerの終了処理
		ParticleManager::GetInstance()->Finalize();

		// ワイヤーフレームの終了処理
		Wireframe::GetInstance()->Finalize();

		// デバッグカメラの終了処理
		DebugCamera::GetInstance()->Finalize();

		// カメラの司令塔の終了処理
		CameraManager::GetInstance()->Finalize();

		// アニメーションパイプラインビルダーの終了処理
		AnimationPipelineBuilder::GetInstance()->Finalize();

		// Object3DCommonの終了処理
		Object3DCommon::GetInstance()->Finalize();

		ModelManager::GetInstance()->Finalize();

		// スプライトマネージャの終了処理
		SpriteManager::GetInstance()->Finalize();

		// BlendStateFactoryの終了処理
		BlendStateFactory::GetInstance()->Finalize();

		// TextureManagerの終了処理
		TextureManager::GetInstance()->Finalize();

		// UAVManagerの終了処理
		UAVManager::GetInstance()->Finalize();

#ifdef USE_IMGUI
		// ImGuiManagerの終了処理
		ImGuiManager::GetInstance()->Finalize();
#endif // USE_IMGUI

		// SRVManagerの終了処理
		SRVManager::GetInstance()->Finalize();

		// DirectX共通クラスの終了処理
		dxCommon_->Finalize();

		// ウィンドウアプリケーションの終了処理
		winApp_->Finalize();
	}


} // namespace Ken4lowEngine
