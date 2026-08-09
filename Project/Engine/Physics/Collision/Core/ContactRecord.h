#pragma once
#include <vector>
#include <cstdint>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                         接触履歴クラス
	/// -------------------------------------------------------------
	class ContactRecord
	{
	public: /// ---------- メンバ関数 ---------- ///

		// 履歴を追加する関数（※重複登録しない）
		void Add(uint32_t number);

		// 履歴を確認する関数
		bool Check(uint32_t number) const;

		// 指定IDを履歴から削除する関数
		void Remove(uint32_t number);

		// 履歴を全削除する関数
		void Clear();

		// 履歴の数を取得する関数
		size_t GetRecordCount() const { return record_.size(); }

	private: /// ---------- メンバ変数 ---------- ///

		// 履歴を記録する変数
		std::vector<uint32_t> record_;
	};

} // namespace Ken4lowEngine
