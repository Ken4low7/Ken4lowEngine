#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{

/// ---------- 平面の構造体 ---------- ///
struct Plane final
{
	Vector3 normal;	//!< 法線
	float distance;	//!< 距離
};

} // namespace Ken4lowEngine
