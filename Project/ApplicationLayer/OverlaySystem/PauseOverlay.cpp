#include "PauseOverlay.h"
#include "SceneManager.h"
#include <SpriteManager.h>
#include <Input.h>
#include <DirectXCommon.h>
#include "GameViewportConstants.h"

using namespace Ken4lowEngine;

/// -------------------------------------------------------------
///					　	オーバーレイを開く処理
/// -------------------------------------------------------------
void PauseOverlay::Open(Ken4lowEngine::SceneManager* sceneManager)
{
	// 基底クラスの Open を呼ぶ（sceneManager_ に保存される想定）
	BaseOverlay::Open(sceneManager);

	// 入力管理オブジェクトの取得
	input_ = K4E::Input::GetInstance();

	// スプライト生成＆レイアウト
	InitializeSprites();

	// 最初は「続きから」選択
	focus_ = 0;
	setEmphasis(btnContinue_.get(), true);
	setEmphasis(btnSettings_.get(), false);
	setEmphasis(btnTitle_.get(), false);
}

/// -------------------------------------------------------------
///					　	スプライト初期化＆レイアウト
/// -------------------------------------------------------------
void PauseOverlay::InitializeSprites()
{
	// Pause UIはWinAppサイズではなく固定内部解像度1920x1080基準で配置する。
	const float screenW = static_cast<float>(GameViewportConstants::Width);
	const float screenH = static_cast<float>(GameViewportConstants::Height);

	// --- 背景暗転 ---
	dim_ = std::make_unique<Sprite>();
	dim_->Initialize(kWhiteTex.c_str());
	dim_->SetAnchorPoint({ 0.0f, 0.0f });
	dim_->SetPosition({ 0.0f, 0.0f });
	dim_->SetSize({ screenW, screenH });
	// ちょっと暗く（黒＋α）
	dim_->SetColor({ 0.0f, 0.0f, 0.0f, 0.5f });

	// --- パネル ---
	panel_ = std::make_unique<Sprite>();
	panel_->Initialize(kPanelTex.c_str());
	panel_->SetAnchorPoint({ 0.5f, 0.5f });
	panel_->SetPosition({ screenW * 0.5f, screenH * 0.5f });
	// 960x640 くらい（少し小さめ）
	panel_->SetSize({ 960.0f, 640.0f });
	panel_->SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });

	// --- ボタン共通 ---
	const float btnW = 300.0f;
	const float btnH = 60.0f;

	const float centerX = screenW * 0.5f;
	const float startY = screenH * 0.5f - 60.0f;	// 真ん中に 3 つ縦並び
	const float gapY = 80.0f;

	auto makeButton = [&](std::unique_ptr<Sprite>& dst, float cx, float cy)
		{
			dst = std::make_unique<Sprite>();
			dst->Initialize(kBtnTex.c_str());
			dst->SetAnchorPoint({ 0.5f, 0.5f });
			dst->SetPosition({ cx, cy });
			dst->SetSize({ btnW, btnH });
			// 通常色（暗めのグレー）
			dst->SetColor({ 0.2f, 0.2f, 0.2f, 0.8f });
		};

	// 再開 / 設定 / タイトル
	makeButton(btnContinue_, centerX, startY);
	makeButton(btnSettings_, centerX, startY + gapY);
	makeButton(btnTitle_, centerX, startY + gapY * 2.0f);

	// クリック判定用の矩形（左上原点）
	rectContinue_ = { centerX - btnW * 0.5f, startY - btnH * 0.5f, btnW, btnH };
	rectSettings_ = { centerX - btnW * 0.5f, startY + gapY - btnH * 0.5f, btnW, btnH };
	rectTitle_ = { centerX - btnW * 0.5f, startY + gapY * 2.0f - btnH * 0.5f, btnW, btnH };
}

/// -------------------------------------------------------------
///					　	更新処理
/// -------------------------------------------------------------
void PauseOverlay::Update()
{
	// スプライトの Update（アニメーション等があれば）
	if (dim_)        dim_->Update();
	if (panel_)      panel_->Update();
	if (btnContinue_)btnContinue_->Update();
	if (btnSettings_)btnSettings_->Update();
	if (btnTitle_)   btnTitle_->Update();

	if (!input_) { return; }

	// キーボードで選択移動
	UpdateFocusByKey();

	// マウスホバーでフォーカス移動
	UpdateFocusByMouse();

	// 決定（Enter / Space / 左クリック）
	bool decideKey =
		input_->TriggerKey(DIK_RETURN) ||
		input_->TriggerKey(DIK_SPACE);
	bool decideMouse = input_->TriggerMouse(0);

	// キーボード決定
	if (decideKey)
	{
		DecideCurrent();
	}
	// マウス決定
	else if (decideMouse)
	{
		auto mousePos = input_->GetMousePosition();

		auto inside = [](const ButtonRect& r, const decltype(mousePos)& p)
			{
				return (p.x >= r.x && p.x <= r.x + r.w &&
					p.y >= r.y && p.y <= r.y + r.h);
			};

		bool clickedAny = false;

		if (inside(rectContinue_, mousePos)) {
			focus_ = 0;
			clickedAny = true;
		}
		else if (inside(rectSettings_, mousePos)) {
			focus_ = 1;
			clickedAny = true;
		}
		else if (inside(rectTitle_, mousePos)) {
			focus_ = 2;
			clickedAny = true;
		}

		// どれかのボタンの中をクリックしたときだけ決定
		if (clickedAny)
		{
			DecideCurrent();
		}
	}

	// フォーカス中のボタンだけ色を変える
	setEmphasis(btnContinue_.get(), focus_ == 0);
	setEmphasis(btnSettings_.get(), focus_ == 1);
	setEmphasis(btnTitle_.get(), focus_ == 2);
}

/// -------------------------------------------------------------
///					　	キーボードでフォーカス移動
/// -------------------------------------------------------------
void PauseOverlay::UpdateFocusByKey()
{
	int prev = focus_;

	if (input_->TriggerKey(DIK_UP) || input_->TriggerKey(DIK_W))
	{
		focus_ = (focus_ + 3 - 1) % 3;	// 上へ
	}
	else if (input_->TriggerKey(DIK_DOWN) || input_->TriggerKey(DIK_S))
	{
		focus_ = (focus_ + 1) % 3;		// 下へ
	}

	if (focus_ != prev)
	{
		// TODO: カーソル移動 SE を鳴らすならここ
	}
}

/// -------------------------------------------------------------
///					　	マウスでフォーカス移動
/// -------------------------------------------------------------
void PauseOverlay::UpdateFocusByMouse()
{
	auto mousePos = input_->GetMousePosition();

	auto inside = [](const ButtonRect& r, const decltype(mousePos)& p)
		{
			return (p.x >= r.x && p.x <= r.x + r.w &&
				p.y >= r.y && p.y <= r.y + r.h);
		};

	if (inside(rectContinue_, mousePos)) {
		focus_ = 0;
	}
	else if (inside(rectSettings_, mousePos)) {
		focus_ = 1;
	}
	else if (inside(rectTitle_, mousePos)) {
		focus_ = 2;
	}
}

/// -------------------------------------------------------------
///					　	現在フォーカス中の項目を決定
/// -------------------------------------------------------------
void PauseOverlay::DecideCurrent()
{
	switch (focus_)
	{
	case 0: // Continue（ゲーム再開）
		// ただ閉じるだけ。GamePlayScene 側で IsClose() を見て Playing に戻る
		close_ = true;
		break;

	case 1: // Settings
		// TODO: 設定オーバーレイを作ったらここで呼び出し
		// ひとまず何もしない（ポーズ画面開きっぱなし）
		break;

	case 2: // Title（タイトルに戻る）

		input_->SetLockCursor(false);
		ShowCursor(true);

		if (sceneManager_)
		{
			sceneManager_->ChangeScene("TitleScene");
		}
		goTitle_ = true;
		close_ = true;
		break;
	}
}

/// -------------------------------------------------------------
///					　	ボタンの強調表示
/// -------------------------------------------------------------
void PauseOverlay::setEmphasis(Sprite* sprite, bool emphasized)
{
	if (!sprite) { return; }

	if (emphasized)
	{
		// オレンジっぽいハイライト
		sprite->SetColor({ 1.0f, 0.6f, 0.2f, 1.0f });
	}
	else
	{
		// 通常は暗めグレー
		sprite->SetColor({ 0.2f, 0.2f, 0.2f, 0.8f });
	}
}

/// -------------------------------------------------------------
///					　		2D描画処理
/// -------------------------------------------------------------
void PauseOverlay::Draw2D()
{
	if (dim_) { dim_->Draw(); }       // 背景の暗転（背面）
	if (panel_) { panel_->Draw(); }     // パネル

	if (btnContinue_) { btnContinue_->Draw(); }
	if (btnSettings_) { btnSettings_->Draw(); }
	if (btnTitle_) { btnTitle_->Draw(); }
}