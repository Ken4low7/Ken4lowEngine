#pragma once

#include <Sprite.h>

#include <memory>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///                    HP（ハート）表示ウィジェット
/// -------------------------------------------------------------
/// - HUDManager から「現在HP/最大HP」を渡して描画するだけの部品
/// - 1ハートあたり hpPerHeart_（既定10）
/// - 1ハートは Full / Half / Empty(Death) の3種類で表現
class HPWidget
{
public:
	/// 初期化（テクスチャ読み込み＆スプライト生成）
	void Initialize(
		const std::string& fullTex = "UI/Icons/heart_full.dds",
		const std::string& halfTex = "UI/Icons/heart_half.dds",
		const std::string& emptyTex = "UI/Icons/heart_death.dds");

	/// 更新（位置反映など）
	void Update();

	/// 描画
	void Draw();

	/// 表示ON/OFF
	void SetVisible(bool v) { isVisible_ = v; }
	bool IsVisible() const { return isVisible_; }

	/// HP設定
	void SetHP(float hp, float maxHp);

	/// ヒット通知（HPが減らないヒットでも揺らしたい場合に呼ぶ）
	/// strength01: 0..1（1が最大）
	void NotifyHit(float strength01 = 1.0f);

	/// 見た目調整
	void SetAnchorTopLeft(const K4E::Vector2& posPx) { anchorPos_ = posPx; }
	void SetIconSize(const K4E::Vector2& sizePx) { iconSize_ = sizePx; rebuildRequested_ = true; }
	void SetPadding(float padPx) { padding_ = padPx; rebuildRequested_ = true; }
	void SetHpPerHeart(float hpPerHeart) { hpPerHeart_ = (hpPerHeart > 0.0f) ? hpPerHeart : 10.0f; rebuildRequested_ = true; }
	void SetMaxHeartsCap(int cap) { maxHeartsCap_ = (cap > 0) ? cap : 1; rebuildRequested_ = true; }

	/// 揺れ設定
	void SetCriticalHeartsThreshold(int hearts) { criticalHeartsThreshold_ = (hearts >= 0) ? hearts : 0; }
	void SetMaxShakePixels(float px) { maxShakePixels_ = (px >= 0.0f) ? px : 0.0f; }

private:
	struct HeartSlot
	{
		std::unique_ptr<K4E::Sprite> full;
		std::unique_ptr<K4E::Sprite> half;
		std::unique_ptr<K4E::Sprite> empty;
	};

	void RebuildSlots(int heartCount);
	void UpdateSlotPositions();

private:
	std::string texFull_;
	std::string texHalf_;
	std::string texEmpty_;

	std::vector<HeartSlot> slots_;

	// データ
	float hp_ = 100.0f;
	float maxHp_ = 100.0f;
	float prevHp_ = 100.0f;

	// 揺れ（トラウマ方式）
	float trauma_ = 0.0f;           // 0..1
	float shakeTime_ = 0.0f;        // 内部位相
	K4E::Vector2 shakeOffset_ = { 0.0f, 0.0f };
	int   criticalHeartsThreshold_ = 3; // 残りハートがこの数以下なら常時揺れ
	float maxShakePixels_ = 10.0f;      // 最大揺れ（px）
	float hitTraumaKick_ = 0.75f;       // ヒット時の追加トラウマ
	float damageTraumaKick_ = 0.55f;    // HP減少時の追加トラウマ
	float traumaDecayPerSec_ = 1.6f;    // トラウマ減衰/秒
	float baselineTraumaAtCritical_ = 0.30f; // 瀕死時の最低トラウマ

	// 表示設定
	bool isVisible_ = true;
	K4E::Vector2 anchorPos_ = { 20.0f, 20.0f }; // 左上基準
	K4E::Vector2 iconSize_ = { 22.0f, 22.0f };
	float padding_ = 6.0f;
	float hpPerHeart_ = 10.0f;
	int   maxHeartsCap_ = 20; // 最大表示数（暴走防止）

	bool rebuildRequested_ = true;
};
