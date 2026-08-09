#pragma once
#include <Sprite.h>

#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine
{
	class Input;
	class TextSpriteDrawer;
}

// ポーズメニューのコマンド
enum class PauseMenuCommand
{
	None,
	Resume,         // ゲームに戻る
	ToStageSelect,  // ステージセレクトへ
	ToTitle         // タイトルへ
};

/// -------------------------------------------------------------
///					ポーズ画面を管理するクラス（Spriteベース）
/// -------------------------------------------------------------
class PauseMenu
{
public: /// ---------- メンバ関数 ---------- ///

	// デストラクタ
	~PauseMenu();

	void Initialize();

	void Open();

	void Close();

	PauseMenuCommand Update(Ken4lowEngine::Input* input);

	void Draw(); // Spriteベース描画（ImGui不使用）

	bool IsOpen() const { return isOpen_; }

private: /// ---------- 内部構造体 ---------- ///

	struct Rect
	{
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
	};

	struct ButtonSprites
	{
		std::unique_ptr<K4E::Sprite> bg;
		std::unique_ptr<K4E::Sprite> border;
		std::unique_ptr<K4E::Sprite> accent;
		Rect rect{};
	};

private: /// ---------- メンバ関数 ---------- ///

	void RebuildLayout();
	void RefreshScreenSizeIfNeeded();
	int HitTestButtonIndex(const K4E::Vector2& mousePos) const;
	void ApplyVisualState();
	void DrawTexts();

private: /// ---------- メンバ変数 ---------- ///

	bool isOpen_ = false;
	bool textReady_ = false; // TextSpriteDrawerの初期化が完了しているか
	int selectedIndex_ = 0;
	float screenWidth_ = 1280.0f;
	float screenHeight_ = 720.0f;
	float panelX_ = 0.0f;
	float panelY_ = 0.0f;
	float panelW_ = 0.0f;
	float panelH_ = 0.0f;

	std::vector<std::string> items_ =
	{
		"ゲームに戻る",
		"ステージ選択",
		"タイトルへ戻る"
	};

	// 背景 / パネル
	std::unique_ptr<K4E::Sprite> overlay_;
	std::unique_ptr<K4E::Sprite> panel_;
	std::unique_ptr<K4E::Sprite> panelBorder_;
	std::unique_ptr<K4E::Sprite> titleLine_;
	std::unique_ptr<K4E::TextSpriteDrawer> textDrawer_;

	// ボタン群（items_ と同じ順番）
	std::vector<ButtonSprites> buttons_;
};
