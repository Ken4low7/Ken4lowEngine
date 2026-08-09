#pragma once
#include "Contact.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         速度補正ソルバー
	/// -------------------------------------------------------------
	class VelocitySolver
	{
	public: /// ---------- メンバ関数 ---------- ///

		// Contact面へ入り込む速度成分を補正する。
		void Resolve(Contact& contact) const;
	};

} // namespace Ken4lowEngine
