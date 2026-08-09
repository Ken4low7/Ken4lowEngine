#pragma once
#include "Vector3.h"
#include "Segment.h"

namespace Ken4lowEngine
{

	/// ---------- カプセル構造体 ---------- ///
	struct Capsule
	{
		Segment segment; // 中心線分
		float radius = 0.1f;

		// 中心点（描画用）
		Vector3 GetCenter() const { return segment.origin + segment.diff * 0.5f; }

		// 高さ（球部分を除いたシリンダー長を含む全長）
		float GetHeight() const { return Vector3::Length(segment.diff); }

		// 軸ベクトル（正規化済み）
		Vector3 GetAxis() const { return Vector3::Normalize(segment.diff); }
	};

} // namespace Ken4lowEngine
