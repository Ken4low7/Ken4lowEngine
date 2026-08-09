#pragma once
#include "Input.h"

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	/// <summary>
	/// 2D ベクトル(x, z)を正規化する。
	/// 長さが極端に小さい場合は 0 扱いにする。
	/// 主に斜め移動時の速度補正に使う。
	/// </summary>
	void NormalizeClamp2(float& x, float& z);
}