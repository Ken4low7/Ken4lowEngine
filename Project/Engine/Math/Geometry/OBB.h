#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{

/// ----------- OBBの構造体 ---------- ///
struct OBB
{
	Vector3 center;			 // 中心点
	Vector3 orientations[3]; // 各軸の向き（正規化済み）
	Vector3 size;			 // 各軸方向の半分の長さ（半サイズ）
};
} // namespace Ken4lowEngine
