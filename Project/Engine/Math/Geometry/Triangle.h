#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{

/// ---------- 三角形の頂点 ---------- ///
struct Triangle final
{
	Vector3 vertices[3]; //!< 頂点
};
} // namespace Ken4lowEngine
