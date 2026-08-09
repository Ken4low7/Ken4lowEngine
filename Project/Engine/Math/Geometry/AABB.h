#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// ---------- AABB構造体 ---------- ///
	struct AABB final
	{
		Vector3 min; // 最小値
		Vector3 max; // 最大値
	};
} // namespace Ken4lowEngine
