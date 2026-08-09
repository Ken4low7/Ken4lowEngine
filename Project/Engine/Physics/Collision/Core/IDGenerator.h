#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///				　		ID生成クラス
	/// -------------------------------------------------------------
	class IDGenerator
	{
	public: /// ---------- メンバ関数 ---------- ///

		// IDを生成
		static uint32_t Generate() { return nextID_++; }

		// IDをリセット
		static void Reset() { nextID_ = 0; }

	private: /// ---------- メンバ変数 ---------- ///

		// 次のID
		static inline uint32_t nextID_ = 0;
	};


} // namespace Ken4lowEngine
