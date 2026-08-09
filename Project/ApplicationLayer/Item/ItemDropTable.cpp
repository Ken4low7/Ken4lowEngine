#include "ItemDropTable.h"

/// -------------------------------------------------------------
///						コンストラクタ
/// -------------------------------------------------------------
ItemDropTable::ItemDropTable()
{
	// 乱数生成器の初期化
	std::random_device rd;
	rng_ = std::mt19937(rd());

	// 0-99 の一様分布
	chanceDist_ = std::uniform_int_distribution<int>(0, 99);
}

/// -------------------------------------------------------------
///				アイテムタイプとその出現重み
/// -------------------------------------------------------------
void ItemDropTable::AddEntry(ItemType itemType, int weight)
{
	// 無効な重みは無視
	if (weight <= 0) return;

	// エントリを追加
	entries_.push_back({ itemType, weight });

	// 重みの合計を更新
	totalWeight_ += weight;
}

/// -------------------------------------------------------------
///						ドロップテーブルをクリア
/// -------------------------------------------------------------
void ItemDropTable::Clear()
{
	entries_.clear(); // エントリをクリア
	totalWeight_ = 0; // 重みの合計をリセット
}

/// -------------------------------------------------------------
///					特定のアイテムタイプを削除
/// -------------------------------------------------------------
void ItemDropTable::RemoveEntry(ItemType itemType)
{
	// 重みの合計をリセット
	totalWeight_ = 0;

	// 指定されたアイテムタイプのエントリを削除
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const DropEntry& entry) {
		return entry.itemType == itemType;
		}), entries_.end());

	// 重みの合計を再計算
	for (const auto& e : entries_) totalWeight_ += e.weight;
}

/// -------------------------------------------------------------
///							ロール
/// -------------------------------------------------------------
bool ItemDropTable::RollForDrop(ItemType& outItemType)
{
	// ドロップ確率チェック
	if (chanceDist_(rng_) >= dropChancePercent_) return false;

	// 重み付きランダム選択
	std::uniform_int_distribution<int> weightDist(0, totalWeight_ - 1);
	int r = weightDist(rng_);

	// 重みの累積和を使って選択
	int accumulated = 0;

	// ここで totalWeight_ が 0 の場合を防ぐ
	for (const auto& entry : entries_)
	{
		// 累積和を更新
		accumulated += entry.weight;

		// ランダム値が累積和を下回ったらそのアイテムを選択
		if (r < accumulated)
		{
			outItemType = entry.itemType; // 選ばれたアイテムタイプを出力
			return true; // アイテムがドロップされた
		}
	}

	// ここに来ることは通常ないが、念のため false を返す
	return false;
}
