#pragma once

/// ---------- アイテムの定義 ---------- ///
enum class ItemType
{
	None = 0,		// アイテムなし
	HealSmall,		// 小回復アイテム
	AmmoSmall,		// 小弾薬アイテム
	NextStageKey,	// 次ステージ解放キー
};
