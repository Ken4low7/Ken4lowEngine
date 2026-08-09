#pragma once
#include "Contact.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         摩擦ソルバー
	/// -------------------------------------------------------------
	class FrictionSolver
	{
	public: /// ---------- メンバ関数 ---------- ///

		// 接触面に沿った速度成分を摩擦で減速する。
		void Resolve(Contact& contact) const;
	};

} // namespace Ken4lowEngine
