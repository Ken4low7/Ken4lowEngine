#pragma once
#include "ItemType.h"
#include <vector>
#include <random>

/// -------------------------------------------------------------
///					アイテムドロップテーブルクラス
/// -------------------------------------------------------------
class ItemDropTable
{
private: /// ---------- 構造体 ---------- ///

	/// ---------- ドロップエントリの構造体 ---------- ///
	struct DropEntry
	{
		ItemType itemType; // アイテムの種類
		int weight;		   // ドロップ確率の重み（合計100を想定）
	};

public: /// ---------- メンバ関数 ---------- ///

	// コンストラクタ
	ItemDropTable();

	// アイテムタイプとその出現重み
	void AddEntry(ItemType itemType, int weight);

	// ドロップテーブルをクリア
	void Clear();

	// 特定のアイテムタイプを削除
	void RemoveEntry(ItemType itemType);

	// ロール
	bool RollForDrop(ItemType& outItemType);

public: /// ---------- セッター ---------- ///

	// ドロップ確率を設定（0-100）
	void SetDropChance(int chancePercent) { dropChancePercent_ = chancePercent; }

private: /// ---------- メンバ変数 ---------- ///

	std::vector<DropEntry> entries_; // ドロップエントリのリスト
	int totalWeight_ = 0;			 // 重みの合計
	int dropChancePercent_ = 100;	 // ドロップ確率（0-100）

	std::mt19937 rng_; // 乱数生成器
	std::uniform_int_distribution<int> chanceDist_; // 0-99
};

