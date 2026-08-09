#pragma once
#include "BaseOverlay.h"
#include <Sprite.h>
#include <Rect.h>
#include <TextSpriteDrawer.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// ---------- 前方宣言 ---------- ///
namespace Ken4lowEngine
{
	class Input;
}

/// -------------------------------------------------------------
///					終了確認オーバーレイクラス
/// 2D矩形Spriteの生成はUiSpriteFactoryへ寄せ、Yes/Noの選択・決定・キャンセル処理はこのOverlay側で管理します。
/// -------------------------------------------------------------
class ConfirmQuitOverlay final : public BaseOverlay
{
public: /// ---------- メンバ関数 ---------- ///

	// コールバック関数の型定義
	using Callback = std::function<void()>;

	// デストラクタ
	~ConfirmQuitOverlay() override;

	// オーバーレイを開く処理
	void Open(Ken4lowEngine::SceneManager* sceneManager) override;

	// 更新処理
	void Update() override;

	// 2D描画処理
	void Draw2D() override;

	// ワールドを一時停止するかどうか
	bool PausesWorld() const override { return true; }

	// コールバック関数の設定
	void SetCallbacks(Callback onYes, Callback onNo) { onYes_ = std::move(onYes); onNo_ = std::move(onNo); }

private: /// ---------- メンバ変数 ---------- ///

	K4E::Input* input_ = nullptr; // 入力管理オブジェクトへのポインタ

	std::unique_ptr<K4E::Sprite> dim_;
	std::unique_ptr<K4E::Sprite> panelBorder_;
	std::unique_ptr<K4E::Sprite> panel_;
	std::vector<std::unique_ptr<K4E::Sprite>> blockTiles_;
	std::unique_ptr<K4E::Sprite> titleLine_;
	std::unique_ptr<K4E::Sprite> btnYesBorder_;
	std::unique_ptr<K4E::Sprite> btnYes_;
	std::unique_ptr<K4E::Sprite> btnNoBorder_;
	std::unique_ptr<K4E::Sprite> btnNo_;
	std::unique_ptr<K4E::TextSpriteDrawer> textDrawer_;

	Rect rYes_{ 0.0f, 0.0f, 300.0f, 84.0f };
	Rect rNo_{ 0.0f, 0.0f, 300.0f, 84.0f };
	int focus_ = 0; // 0:Yes,1:No
	bool textReady_ = false; // TextSpriteDrawerの初期化が完了しているか

	Callback onYes_;
	Callback onNo_;

};
