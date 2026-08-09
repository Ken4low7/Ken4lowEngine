#define NOMINMAX
#include "TitleScene.h"
#include <DirectXCommon.h>
#include <SpriteManager.h>
#include <CameraManager.h>
#include <ImGuiManager.h>
#include "SceneManager.h"
#include <CollisionUtility.h>
#include "Input.h"
#include <Wireframe.h>
#include "GameViewportConstants.h"
#include <LinearInterpolation.h>
#include <AudioManager.h>
#include <PostEffectManager.h>
#include <LightManager.h>
#include <Editor/EditorTransformAccess.h>
#include <GameTimer.h>
#include <FontAtlasLoader.h>
#include <TextSpriteDrawer.h>

#include <algorithm>
#include <utility>

#ifdef USE_IMGUI
#include <Editor/EditorWindowManager.h>
#endif // USE_IMGUI

#include "TitleLoadState.h"
#include "TitleAttractState.h"
#include "TitleLobbyState.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///				　			　初期化処理
/// -------------------------------------------------------------
void TitleScene::Initialize()
{
	LightManager::GetInstance()->ResetToDefaultLighting();

	dxCommon_ = DirectXCommon::GetInstance();
	input_ = Input::GetInstance();
	if (input_)
	{
		// TitleSceneはReleaseでもDebug UIに依存しない操作画面なので、カーソルを通常表示へ戻す。
		input_->SetLockCursor(false);
		input_->SetCursorVisible(true);
	}

	skyBox_ = std::make_unique<SkyBox>();
	skyBox_->Initialize("SkyBox/skybox.dds");

	timers_.state = timers_.idle = 0.0f;
	timers_.inputCooldownLeft = 0.0f;

	// カメラの初期化
	InitializeCamera();

	// タイトルロゴの初期化
	InitializeLogoUI();

	// バトルへボタンの初期化
	InitializeBattleButtonUI();

	// 影の初期化
	InitializeButtonShadowSprite();

	// バトルへボタン文字の初期化
	InitializeBattleButtonText();

	// クリックヒントの初期化
	InitializeClickHintUI();

	// 段階ロードの初期状態
	loadStep_ = 0;
	isLoadReady_ = false;

	// まだ重い生成はしない
	skyBox_.reset();
	terrain_.reset();
	currentState_.reset();

	state_ = State::Loading;
}


/// -------------------------------------------------------------
///				　			　 更新処理
/// -------------------------------------------------------------
void TitleScene::Update()
{
	// デバッグ更新
	UpdateDebug();

	// ロード中は軽く回すだけ
	if (state_ == State::Loading)
	{
		if (skyBox_) skyBox_->Update();
		if (terrain_) terrain_->Update();
		return;
	}

	// 地形更新
	if (terrain_) terrain_->Update();

	// スカイボックス更新
	if (skyBox_) skyBox_->Update();

	if (isDebugCamera_) return; // デバッグカメラ中はポーズ無効

	// 時間更新
	const float dt = K4E::GameTimer::GetInstance()->GetDeltaTime();
	timers_.state += dt;

	// 入力クールダウン更新
	if (timers_.inputCooldownLeft > 0.0f) { timers_.inputCooldownLeft = std::max(0.0f, timers_.inputCooldownLeft - dt); }

	// ====== すでにオーバーレイが出ているならそれを最優先で更新＆早期return ======
	if (quitOverlay_)
	{
		quitOverlay_->Update();                                           // ConfirmQuitOverlay は自前でマウス/キー処理を持つ
		if (quitOverlay_->IsClose()) { quitOverlay_.reset(); }            // 閉じられたら破棄
		skyBox_->Update();
		return;                                                           // オーバーレイ中は他の入力・遷移を止める
	}

	// ====== トリガー（ESC / Q）で生成して Open(sceneManager_) ======
	if (input_->TriggerKey(DIK_ESCAPE))
	{
		quitOverlay_ = std::make_unique<ConfirmQuitOverlay>();
		quitOverlay_->Open(sceneManager_);                                // BaseOverlay::Open で SceneManager を注入
		// Yes: アプリ終了 / No: 何もしない（Close は Overlay 側が呼ぶ）
		quitOverlay_->SetCallbacks([]() {
#ifdef _WIN32
			PostQuitMessage(0); // Windowsアプリ終了
#else
			std::exit(0); // 他プラットフォームでは標準終了
#endif
			},
			[]()
			{}
		);
	}

	// ----- 状態更新共通処理 -----
	if (currentState_)
	{
		// ステートクラスに丸投げ
		currentState_->Update(this, dt);
	}

	UpdateLightViewProjection();
	UpdateShadowMatrices();
}


/// -------------------------------------------------------------
///				 	Editor中の更新処理
/// -------------------------------------------------------------
void TitleScene::UpdateEditor(float deltaTime)
{
	(void)deltaTime;

	// Edit/Pause中はTitleのクリック遷移やEsc終了確認を処理せず、描画に必要な更新だけ行う。
	if (terrain_) terrain_->Update();
	if (skyBox_) skyBox_->Update();

	UpdateLightViewProjection();
	UpdateShadowMatrices();
}

/// -------------------------------------------------------------
///				　	3Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw3DObjects()
{
#pragma region オブジェクト3Dの描画

	if (skyBox_) skyBox_->Draw();

	if (terrain_) terrain_->Draw();

#pragma endregion

}

void TitleScene::DrawShadowObjects()
{
	if (terrain_) terrain_->DrawShadow();
}

/// -------------------------------------------------------------
///				　	2Dオブジェクトの描画
/// -------------------------------------------------------------
void TitleScene::Draw2DSprites()
{
#pragma region 背景の描画（後面）

	// 背景用の共通描画設定（後面）
	SpriteManager::GetInstance()->SetRenderSetting_Background();


#pragma endregion


#pragma region UIの描画（前面）
	// UI用の共通描画設定
	SpriteManager::GetInstance()->SetRenderSetting_UI();

	// タイトル系（TitleAttract と ToTitle の間だけロゴを表示）
	if (state_ == State::TitleAttract || state_ == State::ToTitle || logoUI_.exitLeft > 0.0f)
	{
		if (logoSprite_)
		{
			logoSprite_->SetColor({ 1,1,1,logoUI_.alpha });
			const Vector2 sz = { logoUI_.baseSize.x * logoUI_.scale, logoUI_.baseSize.y * logoUI_.scale };
			logoSprite_->SetSize(sz);
			logoSprite_->Draw();

			if (clickHintUI_.isVisible && clickHintUI_.hintSprite) { clickHintUI_.hintSprite->Draw(); }
		}
	}

	// ロビー待機状態になるまではバトルボタンを描画せず、タイトルクリック直後に先出しされないようにする。
	if (state_ == State::LobbyIdle)
	{
		if (battleButtonUI_.btnShadow) { battleButtonUI_.btnShadow->Draw(); } // ← 影を先に
		if (battleButtonUI_.btnBorder) { battleButtonUI_.btnBorder->Draw(); }
		if (battleButtonUI_.btnSprite) { battleButtonUI_.btnSprite->Draw(); } // ← ボタン本体
		if (battleButtonUI_.btnAccentLeft) { battleButtonUI_.btnAccentLeft->Draw(); }
		if (battleButtonUI_.btnAccentRight) { battleButtonUI_.btnAccentRight->Draw(); }
		DrawBattleButtonText();
	}

	// ====== 最後にオーバーレイを最前面へ重ね描き ======
	if (quitOverlay_)
	{
		quitOverlay_->Draw2D();
	}

#pragma endregion
}


/// -------------------------------------------------------------
///				　			　 終了処理
/// -------------------------------------------------------------
void TitleScene::Finalize()
{
	AudioManager::GetInstance()->StopBGM();

	// ステート固有の終了処理だけを流し、所有オブジェクトの破棄はSceneManagerによるScene破棄へ任せる。
	if (currentState_) { currentState_->Exit(this); }

	if (battleButtonTextDrawer_) { battleButtonTextDrawer_->Finalize(); }
	battleButtonTextReady_ = false;

	camera_ = nullptr;
	input_ = nullptr;
	dxCommon_ = nullptr;
}


/// -------------------------------------------------------------
///				　		　ImGui描画処理
/// -------------------------------------------------------------
void TitleScene::DrawImGui()
{
#ifdef USE_IMGUI
	// WindowメニューのTitle Debug表示フラグを×ボタン状態と共有する
	auto& editorWindowState = EditorWindowManager::GetInstance()->GetWindowState();
	if (editorWindowState.showTitleDebug)
	{
		ImGui::Begin("Title Debug", &editorWindowState.showTitleDebug);
		const char* stateNames =
			state_ == State::TitleAttract ? "TitleAttract" :
			state_ == State::TransitionToLobby ? "TransitionToLobby" :
			state_ == State::LobbyIdle ? "LobbyIdle" :
			state_ == State::Loading ? "Loading" : "";
		ImGui::Text("State: %s", stateNames);
		ImGui::Text("Idle: %.1fs / Return: %.0fs", timers_.idle, timers_.returnSeconds);
		ImGui::End();
	}

	// WindowメニューのLight Editor表示フラグをTitleシーン側のライトUIにも共有する
	LightManager::GetInstance()->DrawImGui(&editorWindowState.showLightEditor);
#endif // USE_IMGUI
}

void TitleScene::StartLoad()
{
	loadStep_ = 0;
	isLoadReady_ = false;
	state_ = State::Loading;
}

void TitleScene::UpdateLoad()
{
	switch (loadStep_)
	{
	case 0:
		// SkyBox 読み込み
		skyBox_ = std::make_unique<SkyBox>();
		skyBox_->Initialize("SkyBox/skybox.dds");
		++loadStep_;
		break;

	case 1:
		// 地形読み込み
		terrain_ = std::make_unique<Object3D>();
		terrain_->Initialize("Stages/lobby.gltf");
		++loadStep_;
		break;

	case 2:
		// 最初のステートに入る
		ChangeState(std::make_unique<TitleAttractState>());
		state_ = State::TitleAttract;
		isLoadReady_ = true;
		++loadStep_;
		break;

	default:
		break;
	}
}

bool TitleScene::IsReadyToStartUncover() const
{
	return isLoadReady_;
}

void TitleScene::UpdateLightViewProjection()
{
	// 今は新規シーンで最小確認したいので、中心は原点付近固定でOK
	shadowCenter_ = { 0.0f, 1.0f, 0.0f };

	lightViewProjection_ = Matrix4x4::MakeLightViewProjection(
		lightDirection_,
		shadowCenter_,
		shadowDistance_,
		orthoHalfWidth_,
		orthoHalfHeight_,
		nearZ_,
		farZ_
	);
}

void TitleScene::UpdateShadowMatrices()
{
	if (terrain_) { terrain_->UpdateShadowMatrix(lightViewProjection_); }
}

/// -------------------------------------------------------------
///				　		　カメラ初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeCamera()
{
	// カメラの生成と初期化
	camera_ = CameraManager::GetInstance()->GetMainCamera();
	if (camera_)
	{
		// ロビー用の初期位置にセット
		camera_->SetTranslate({ orbitState_.center.x, orbitState_.center.y, orbitState_.center.z - orbitState_.radius });
		orbitState_.lastPitch = -0.10f; orbitState_.lastYaw = std::numbers::pi_v<float>;
		camera_->SetRotate({ orbitState_.lastPitch, orbitState_.lastYaw, 0.0f });
		camera_->Update();
	}
}

/// -------------------------------------------------------------
///				　		　ロゴUI初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeLogoUI()
{
	logoUI_.scale = 0.9f;   // 少し小さく出して拡大
	logoUI_.showLeft = logoUI_.showDelay;
	logoUI_.exitLeft = 0.0f;

	// ロゴスプライトの生成
	logoSprite_ = std::make_unique<Sprite>();
	logoSprite_->Initialize("UI/Common/logo_rittai_sensen.dds");
	logoUI_.baseSize = logoSprite_->GetSize();
	logoUI_.baseSize *= 0.7f; // 元画像が大きい場合は適宜縮小
	logoSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	logoSprite_->SetPosition({ GameViewportConstants::Width * 0.5f, GameViewportConstants::Height * 0.25f }); // Title UIは固定内部解像度1920x1080基準にする。
}

/// -------------------------------------------------------------
///				　	バトルボタンUI初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeBattleButtonUI()
{
	// UI生成画像に依存しないよう、白スプライトの矩形とTextSpriteDrawerでバトルボタンを構築する。
	battleButtonUI_.btnSprite = std::make_unique<Sprite>();
	battleButtonUI_.btnSprite->Initialize("Effects/white.dds");
	battleButtonUI_.btnSprite->SetAnchorPoint(battleButtonUI_.anchor);

	battleButtonUI_.btnBorder = std::make_unique<Sprite>();
	battleButtonUI_.btnBorder->Initialize("Effects/white.dds");
	battleButtonUI_.btnBorder->SetAnchorPoint(battleButtonUI_.anchor);

	battleButtonUI_.btnAccentLeft = std::make_unique<Sprite>();
	battleButtonUI_.btnAccentLeft->Initialize("Effects/white.dds");
	battleButtonUI_.btnAccentLeft->SetAnchorPoint({ 0.5f, 0.5f });

	battleButtonUI_.btnAccentRight = std::make_unique<Sprite>();
	battleButtonUI_.btnAccentRight->Initialize("Effects/white.dds");
	battleButtonUI_.btnAccentRight->SetAnchorPoint({ 0.5f, 0.5f });

	battleButtonUI_.position = { GameViewportConstants::Width * 0.5f, GameViewportConstants::Height * 0.75f }; // Title UIは固定内部解像度1920x1080基準にする。

	battleButtonUI_.btnSprite->SetPosition(battleButtonUI_.position);
	battleButtonUI_.btnSprite->SetSize(battleButtonUI_.size);
	battleButtonUI_.btnSprite->SetColor({ 0.36f, 0.92f, 0.52f, 0.94f });
	battleButtonUI_.btnSprite->Update();

	battleButtonUI_.btnBorder->SetPosition(battleButtonUI_.position);
	battleButtonUI_.btnBorder->SetSize({ battleButtonUI_.size.x + 10.0f, battleButtonUI_.size.y + 10.0f });
	battleButtonUI_.btnBorder->SetColor({ 0.12f, 0.45f, 0.24f, 0.95f });
	battleButtonUI_.btnBorder->Update();

	const float accentOffsetX = battleButtonUI_.size.x * 0.5f - 28.0f;
	battleButtonUI_.btnAccentLeft->SetPosition({ battleButtonUI_.position.x - accentOffsetX, battleButtonUI_.position.y });
	battleButtonUI_.btnAccentLeft->SetSize({ 16.0f, battleButtonUI_.size.y - 34.0f });
	battleButtonUI_.btnAccentLeft->SetColor({ 0.92f, 1.0f, 0.70f, 0.95f });
	battleButtonUI_.btnAccentLeft->Update();

	battleButtonUI_.btnAccentRight->SetPosition({ battleButtonUI_.position.x + accentOffsetX, battleButtonUI_.position.y });
	battleButtonUI_.btnAccentRight->SetSize({ 16.0f, battleButtonUI_.size.y - 34.0f });
	battleButtonUI_.btnAccentRight->SetColor({ 0.92f, 1.0f, 0.70f, 0.95f });
	battleButtonUI_.btnAccentRight->Update();
}

/// -------------------------------------------------------------
///				　	ボタン影スプライト初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeButtonShadowSprite()
{
	// 影スプライトも作成
	battleButtonUI_.btnShadow = std::make_unique<Sprite>();
	battleButtonUI_.btnShadow->Initialize("Effects/white.dds");
	battleButtonUI_.btnShadow->SetAnchorPoint(battleButtonUI_.anchor);
	battleButtonUI_.btnShadow->SetPosition({ battleButtonUI_.position.x, battleButtonUI_.position.y + 8.0f }); // 影は少し下
	battleButtonUI_.btnShadow->SetSize({ battleButtonUI_.size.x * 1.03f, battleButtonUI_.size.y * 1.03f }); // わずかに大きく
	battleButtonUI_.btnShadow->SetColor({ 0, 0, 0, 0.32f }); // 半透明の黒
	battleButtonUI_.btnShadow->Update();
}

void TitleScene::InitializeBattleButtonText()
{
	battleButtonTextDrawer_ = std::make_unique<TextSpriteDrawer>();
	battleButtonTextReady_ = false;
	try
	{
		auto fontDefJP = FontAtlasLoader::LoadFromJson(
			"UI/Font/JP/DotGothic16-Regular_atlas.dds",
			"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
			32.0f,
			32.0f,
			U'?'
		);
		battleButtonTextDrawer_->Initialize(fontDefJP);
		battleButtonTextReady_ = true;
	}
	catch (...)
	{
		battleButtonTextReady_ = false;
	}
}

/// -------------------------------------------------------------
///				　	クリックヒントUI初期化処理
/// -------------------------------------------------------------
void TitleScene::InitializeClickHintUI()
{
	clickHintUI_.hintSprite = std::make_unique<Sprite>();
	clickHintUI_.hintSprite->Initialize("UI/Common/ui_click_hint.dds");
	clickHintUI_.hintSprite->SetAnchorPoint({ 0.5f, 0.0f });      // 中央上
	// ここは今の 1/10 スケール指定のままでOK
	clickHintUI_.hintSprite->SetPosition({ GameViewportConstants::Width * 0.5f + clickHintUI_.offset.x, GameViewportConstants::Height * 0.25f + clickHintUI_.offset.y }); // Title UIは固定内部解像度1920x1080基準にする。
	clickHintUI_.hintSprite->SetSize({ 153.6f, 102.4f });       // 元画像が1536x1024pxなので1/10スケール
	clickHintUI_.baseSize = clickHintUI_.hintSprite->GetSize(); // 基準サイズを保存
}

void TitleScene::DrawBattleButtonText()
{
	if (!battleButtonTextReady_ || !battleButtonTextDrawer_)
	{
		return;
	}

	const float appear = std::clamp(battleButtonUI_.appearAnim, 0.0f, 1.0f);
	const float appearEase = 1.0f - ((1.0f - appear) * (1.0f - appear) * (1.0f - appear));
	const float appearScale = 0.72f + 0.28f * appearEase;
	const float buttonScale =
		(1.0f + battleButtonUI_.scaleHover * battleButtonUI_.hoverAnim) -
		(battleButtonUI_.scalePress * battleButtonUI_.pressAnim);
	const float scale = buttonScale * appearScale;
	const Vector2 textPos = {
		battleButtonUI_.position.x,
		battleButtonUI_.position.y + battleButtonUI_.pressOffsetPx * battleButtonUI_.pressAnim + battleButtonUI_.appearOffsetPx * (1.0f - appearEase) - 12.0f
	};

	battleButtonTextDrawer_->Reset();
	battleButtonTextDrawer_->SetLetterSpacing(3.0f);
	battleButtonTextDrawer_->SetLineSpacing(6.0f);
	battleButtonTextDrawer_->SetScale(1.30f * scale);
	battleButtonTextDrawer_->SetColor({ 0.05f, 0.08f, 0.06f, appearEase });
	battleButtonTextDrawer_->DrawTextCentered("バトルへ", textPos);
}

/// -------------------------------------------------------------
///				　　ステート変更処理の共通化
/// -------------------------------------------------------------
void TitleScene::ChangeState(std::unique_ptr<ITitleSceneState> newState)
{
	// 今のステートから抜ける
	if (currentState_)
	{
		currentState_->Exit(this);
	}

	// ステート差し替え
	currentState_ = std::move(newState);

	// 新しいステートに入る
	if (currentState_)
	{
		currentState_->Enter(this);
	}
}


/// -------------------------------------------------------------
///				カメラの確保（なければデフォルトを取得）
/// -------------------------------------------------------------
Camera* TitleScene::EnsureCamera()
{
	if (!camera_)
	{
		camera_ = CameraManager::GetInstance()->GetMainCamera();
	}
	return camera_;
}

/// -------------------------------------------------------------
///				　	デバッグ用更新（キー入力など）
/// -------------------------------------------------------------
void TitleScene::UpdateDebug()
{
#ifdef _DEBUG
	if (input_->TriggerKey(DIK_F9))
	{
		const bool next = !CameraManager::GetInstance()->IsUsingDebugCamera();

		CameraManager::GetInstance()->SetUseDebugCamera(next);
		if (skyBox_) skyBox_->SetDebugCamera(next);
		Wireframe::GetInstance()->SetDebugCamera(next);
		isDebugCamera_ = next;
	}
#endif // _DEBUG
}
void TitleScene::CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& outObjects)
{
	const auto addObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
	{
		// Transform未対応の管理項目はDetailsで理由だけを表示し、編集入口を持たせない。
		Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "TitleScene" };
		object.inspectorType = Ken4lowEngine::EditorInspectorType::ManagerInfo;
		outObjects.push_back(std::move(object));
	};
	const auto addLightObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName)
	{
		// LightManagerの先頭ライトを安全なindex指定でDetails編集へ公開する。
		Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "TitleScene" };
		object.inspectorType = Ken4lowEngine::EditorInspectorType::PunctualLights;
		outObjects.push_back(std::move(object));
	};
	const auto addCameraObject = [&outObjects](uint64_t id, const char* displayName, const char* typeName, K4E::Camera* camera)
	{
		Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "TitleScene" };
		object.inspectorType = Ken4lowEngine::EditorInspectorType::Transform;
		if (camera)
		{
			object.canEditTransform = true;
			object.readTransform = [camera](Ken4lowEngine::EditorTransform& transform)
			{
				if (!camera)
				{
					return false;
				}
				transform.position = camera->GetTranslate();
				transform.rotation = camera->GetRotate();
				transform.scale = camera->GetScale();
				return true;
			};
			object.writeTransform = [camera](const Ken4lowEngine::EditorTransform& transform)
			{
				if (!camera)
				{
					return;
				}
				camera->SetTranslate(transform.position);
				camera->SetRotate(transform.rotation);
				camera->SetScale(transform.scale);
				camera->Update();
			};
		}
		outObjects.push_back(std::move(object));
	};
	const auto fillSpriteTransform = [](K4E::Sprite* sprite, Ken4lowEngine::EditorTransform& transform)
	{
		if (!sprite)
		{
			return false;
		}
		const K4E::Vector2& position = sprite->GetPosition();
		const K4E::Vector2& size = sprite->GetSize();
		transform.position = { position.x, position.y, 0.0f };
		transform.rotation = { 0.0f, 0.0f, sprite->GetRotation() };
		transform.scale = { size.x, size.y, 1.0f };
		return true;
	};
	const auto applySpriteTransform = [](K4E::Sprite* sprite, const Ken4lowEngine::EditorTransform& transform)
	{
		if (!sprite)
		{
			return;
		}
		sprite->SetPosition({ transform.position.x, transform.position.y });
		sprite->SetRotation(transform.rotation.z);
		sprite->SetSize({ transform.scale.x, transform.scale.y });
		sprite->Update();
	};
	const auto addSpriteObject = [&outObjects, fillSpriteTransform, applySpriteTransform](
		uint64_t id,
		const char* displayName,
		const char* typeName,
		K4E::Sprite* sprite,
		auto onTransformApplied)
	{
		Ken4lowEngine::EditorObjectInfo object{ id, displayName, typeName, "TitleScene" };
		object.inspectorType = Ken4lowEngine::EditorInspectorType::Transform;
		if (sprite)
		{
			object.canEditTransform = true;
			object.readTransform = [sprite, fillSpriteTransform](Ken4lowEngine::EditorTransform& transform)
			{
				return fillSpriteTransform(sprite, transform);
			};
			object.writeTransform = [sprite, applySpriteTransform, onTransformApplied](const Ken4lowEngine::EditorTransform& transform)
			{
				applySpriteTransform(sprite, transform);
				onTransformApplied(transform);
			};
		}
		outObjects.push_back(std::move(object));
	};

	// Outlinerは編集対象の入口として、TitleSceneを構成する主要カテゴリだけを安定IDで列挙する。
	addObject(Ken4lowEngine::MakeStableEditorObjectId("TitleScene.Root"), "Title Root", "Scene Root");
	addCameraObject(Ken4lowEngine::MakeStableEditorObjectId("TitleScene.Camera"), "Camera", camera_ ? "Camera" : "Camera (pending)", camera_);
	addLightObject(Ken4lowEngine::MakeStableEditorObjectId("TitleScene.PunctualLights"), "Punctual Lights", "Light Manager / Punctual Lights");
	addSpriteObject(
		Ken4lowEngine::MakeStableEditorObjectId("TitleScene.UIRoot"),
		"UI Root",
		logoSprite_ ? "Sprite UI" : "Sprite UI (pending)",
		logoSprite_.get(),
		[this](const Ken4lowEngine::EditorTransform& transform)
		{
			logoUI_.baseSize = { transform.scale.x, transform.scale.y };
		});
	addSpriteObject(
		Ken4lowEngine::MakeStableEditorObjectId("TitleScene.ClickText"),
		"Click Text / Click Sprite",
		clickHintUI_.hintSprite ? "Sprite UI" : "Sprite UI (pending)",
		clickHintUI_.hintSprite.get(),
		[this](const Ken4lowEngine::EditorTransform& transform)
		{
			clickHintUI_.baseSize = { transform.scale.x, transform.scale.y };
			if (logoSprite_)
			{
				const K4E::Vector2& logoPosition = logoSprite_->GetPosition();
				clickHintUI_.offset = { transform.position.x - logoPosition.x, transform.position.y - logoPosition.y };
			}
		});
	{
		Ken4lowEngine::EditorObjectInfo fadeObject{ Ken4lowEngine::MakeStableEditorObjectId("TitleScene.FadeManager"), "FadeManager", "Fade Manager", "TitleScene" };
		fadeObject.inspectorType = Ken4lowEngine::EditorInspectorType::FadeManager;
		fadeObject.drawInspector = [this]()
		{
			// BaseSceneへ注入されたSceneManagerから遷移演出を参照する。
			if (sceneManager_)
			{
				if (auto* sceneTransition = sceneManager_->GetSceneTransition())
				{
					sceneTransition->DrawInspectorContent();
				}
			}
		};
		outObjects.push_back(std::move(fadeObject));
	}
}
