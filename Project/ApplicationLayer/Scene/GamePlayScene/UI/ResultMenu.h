#pragma once
#include "Sprite.h"
#include "TextSpriteDrawer.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>
#include <string>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	class Input;
}

enum class ResultMenuMode
{
	GameClear,
	GameOver,
};

enum class ResultMenuCommand
{
	None,
	NextStage,
	Retry,
	ToTitle,
};

/// -------------------------------------------------------------
///　　　　　　リザルトメニュークラス
/// -------------------------------------------------------------
class ResultMenu
{
private: /// ---------- 構造体 ---------- ///

	struct Button
	{
		std::unique_ptr<Ken4lowEngine::Sprite> border;
		std::unique_ptr<Ken4lowEngine::Sprite> body;
		std::unique_ptr<Ken4lowEngine::Sprite> accentLeft;
		std::unique_ptr<Ken4lowEngine::Sprite> accentRight;
		Ken4lowEngine::Vector2 basePosition{ 0.0f, 0.0f };
		Ken4lowEngine::Vector2 baseSize{ 0.0f, 0.0f };
		bool selected = false;
		float textScale = 0.78f;
		Ken4lowEngine::Vector4 textColor{ 0.90f, 0.88f, 0.76f, 1.0f };
		std::string text;
	};

public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~ResultMenu();

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update();

	// 開く処理（modeに応じた見た目の切り替えも行う）
	void Open(ResultMenuMode mode);

	// 終了処理
	void Close();

	// 更新処理（入力に応じたコマンドを返す）
	ResultMenuCommand Update(Ken4lowEngine::Input* input);

	// 描画処理
	void Draw();

	// メニューが開いているかどうか
	bool IsOpen() const { return isOpen_; }

private: /// ---------- メンバ関数 ---------- ///

	// ボタンの見た目を更新（selected: 選択中かどうか）
	void UpdateButtonVisual(Button& button, bool selected, const Ken4lowEngine::Vector4& accentColor);

	// 現在選択中のボタン状態を見た目へ反映する
	void RefreshButtonVisuals();

	// 表示中のボタン数を取得する
	int GetVisibleButtonCount() const;

	// マウス位置から表示中ボタンの選択番号を取得する
	int HitTestVisibleButtonIndex(const Ken4lowEngine::Vector2& mousePos) const;

	// 表示中ボタン番号からコマンドを取得する
	ResultMenuCommand GetCommandByVisibleIndex(int index) const;

	// ボタンを描画する
	void DrawButton(const Button& button) const;

	// TextSpriteDrawerでリザルト文字を描画する
	void DrawTexts();

	// マウスが指定した矩形内にあるかどうか
	bool IsMouseInside(const Ken4lowEngine::Vector2& mousePos, const Ken4lowEngine::Vector2& center, const Ken4lowEngine::Vector2& size) const;

private: /// ---------- メンバ変数 ---------- ///

	// メニューが開いているかどうか
	bool isOpen_ = false;

	// TextSpriteDrawerの初期化が完了しているか
	bool textReady_ = false;

	// 現在のモード
	ResultMenuMode mode_ = ResultMenuMode::GameClear;

	// 現在選択している表示中ボタン番号
	int selectedIndex_ = 0;

	// 選択中ボタンの点滅・拡縮に使う時間
	float selectionAnimTime_ = 0.0f;

	// 常に表示するスプライト
	std::unique_ptr<Ken4lowEngine::Sprite> backdrop_;
	std::unique_ptr<Ken4lowEngine::Sprite> headerBorder_;
	std::unique_ptr<Ken4lowEngine::Sprite> headerBody_;
	std::unique_ptr<Ken4lowEngine::Sprite> headerLine_;
	std::unique_ptr<Ken4lowEngine::TextSpriteDrawer> textDrawer_;

	// ボタンはモードによって表示/非表示を切り替える
	Button nextStageButton_;
	Button retryButton_;
	Button titleButton_;
};
