#include "ContactRecord.h"
#include <algorithm>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                     履歴を追加する処理
	/// -------------------------------------------------------------
	void ContactRecord::Add(uint32_t number)
	{
		// 既に登録済みなら何もしない（多段ヒット防止）
		if (Check(number)) { return; }
		record_.push_back(number);
	}

	/// -------------------------------------------------------------
	///                     履歴を確認する処理
	/// -------------------------------------------------------------
	bool ContactRecord::Check(uint32_t number) const
	{
		return std::any_of(record_.begin(), record_.end(), [number](uint32_t log) { return log == number; });
	}

	/// -------------------------------------------------------------
	///                     履歴を削除する処理
	/// -------------------------------------------------------------
	void ContactRecord::Remove(uint32_t number)
	{
		record_.erase(std::remove(record_.begin(), record_.end(), number), record_.end());
	}

	/// -------------------------------------------------------------
	///                     履歴を全削除する処理
	/// -------------------------------------------------------------
	void ContactRecord::Clear()
	{
		record_.clear();
	}

} // namespace Ken4lowEngine
