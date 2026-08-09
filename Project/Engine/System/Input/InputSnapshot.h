#pragma once
#include "Input.h"
#include "PlayerInputSnapshot.h"

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	/// <summary>
	/// 2D ベクトル(x, z)を正規化する。
	/// 長さが極端に小さい場合は 0 扱いにする。
	/// 主に斜め移動時の速度補正に使う。
	/// </summary>
	void NormalizeClamp2(float& x, float& z);

	/// <summary>
	/// 毎フレームの入力状態から InputSnapshot を生成する。
	/// Player 側はこのスナップショットだけを見ればよいように、
	/// 生の入力をゲーム用データへ集約する。
	/// </summary>
	InputSnapshot BuildInputSnapshot(K4E::Input& input);
}