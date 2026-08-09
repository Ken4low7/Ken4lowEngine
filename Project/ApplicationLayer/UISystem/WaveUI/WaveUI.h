#pragma once
#include "Sprite.h"
#include "Vector2.h"
#include "NumberSpriteDrawer.h"

#include <string>
#include <memory>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///				　		ウェーブUIクラス
/// -------------------------------------------------------------
class WaveUI
{
public: /// ---------- 構造体 ---------- ///

	struct DisplayState
	{
		int currentWave = 1; // 現在のウェーブ番号（1始まり）
		int totalWaves = 1; // 合計ウェーブ数
		bool isWaveInProgress = false; // 現在ウェーブがスポーン中かどうか
		bool isWaitingNextWave = false; // 次のウェーブスポーン待ちかどうか
		bool isAllWavesCleared = false; // すべてのウェーブがクリアされたかどうか
	};


public: /// ---------- メンバ関数 ---------- ///

	// 初期化処理
	void Initialize();

	// 更新処理
	void Update(float deltaTime);

	// 描画処理
	void Draw();

public: /// ---------- アクセッサ ---------- ///

	void SetDisplayState(const DisplayState& state) { displayState_ = state; }

	void NotifyWaveStarted(int waveNumber, bool isFinalWave);
	void NotifyAllWavesCleared();

	void SetVisible(bool visible) { visible_ = visible; }
	bool IsVisible() const { return visible_; }

private: /// ---------- メンバ関数 ---------- ///

	void UpdateWaveBanner(float deltaTime);

private: /// ---------- メンバ変数 ---------- ///

	DisplayState displayState_{};
	bool visible_ = true;

	// 常時表示
	std::unique_ptr<K4E::Sprite> waveLabelSprite_ = nullptr; // ウェーブバナーの背景スプライト
	std::unique_ptr<K4E::NumberSpriteDrawer> numberDrawer_;

	// 演出用
	std::unique_ptr<K4E::Sprite> waveStartBannerSprite_;
	std::unique_ptr<K4E::Sprite> finalWaveBannerSprite_;
	std::unique_ptr<K4E::Sprite> clearBannerSprite_;

	float bannerTimer_ = 0.0f;
	float bannerDuration_ = 2.0f;

	bool showWaveStartBanner_ = false;
	bool showFinalWaveBanner_ = false;
	bool showClearBanner_ = false;
};

