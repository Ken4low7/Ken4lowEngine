#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{

/// ---------- 球の構造体 ---------- ///
struct Sphere final
{
	Vector3 center;	//!<中心点
	float radius;	//!<半径
};
} // namespace Ken4lowEngine
