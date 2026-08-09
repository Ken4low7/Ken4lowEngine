#pragma once
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <array>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///          フラスタムのワイヤーフレーム描画クラス
	/// -------------------------------------------------------------
	class FrustumDebugRenderer
	{
	public: /// ---------- メンバ関数 ---------- ///

		// 8点のコーナーを結ぶ線分を描画する処理
		std::array<Vector3, 8> BuildCorners(const Matrix4x4& viewProjection) const;

		// 描画処理
		void Draw(const Matrix4x4& viewProjection, const Vector4& color) const;
	};
}
