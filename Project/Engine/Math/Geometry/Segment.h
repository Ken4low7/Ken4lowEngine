#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{

	/// ---------- 線分の構造体 ---------- ///
	struct Segment final
	{
		Vector3 origin; // 始点
		Vector3 diff;   // 始点から終点への差分
	};

	// Line Ray Segment
} // namespace Ken4lowEngine
