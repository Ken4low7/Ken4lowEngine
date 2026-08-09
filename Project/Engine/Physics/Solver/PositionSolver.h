#pragma once
#include "Contact.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         位置補正ソルバー
	/// -------------------------------------------------------------
	class PositionSolver
	{
	public: /// ---------- メンバ関数 ---------- ///

		// Contact情報を使ってCollider同士のめり込みを補正する。
		void Resolve(Contact& contact) const;
	};

} // namespace Ken4lowEngine
