#pragma once

#include <algorithm>

#include "Vector/Vector3.h"

namespace Ken4lowEngine::MathUtil
{
	/// <summary>
	/// XZ平面上の2点間距離を返します。
	/// Y成分を無視する距離計算を共通化し、既存のVector3::LengthXZと同じ結果を返します。
	/// </summary>
	/// <param name="a">始点。</param>
	/// <param name="b">終点。</param>
	/// <returns>XZ平面上の距離。</returns>
	[[nodiscard]] inline float DistanceXZ(const Vector3& a, const Vector3& b)
	{
		return Vector3::LengthXZ(b - a);
	}

	/// <summary>
	/// XZ平面上で、点から線分までの最短距離を返します。
	/// MeleeEnemyで使っていた退化線分の扱いを維持するため、線分長の2乗がepsilon以下なら端点aとの距離を返します。
	/// </summary>
	/// <param name="point">距離を測る点。</param>
	/// <param name="a">線分の始点。</param>
	/// <param name="b">線分の終点。</param>
	/// <param name="epsilon">線分を退化扱いにするしきい値。</param>
	/// <returns>点から線分までのXZ平面上の最短距離。</returns>
	[[nodiscard]] inline float DistancePointToSegmentXZ(
		const Vector3& point,
		const Vector3& a,
		const Vector3& b,
		float epsilon = 0.0001f)
	{
		const Vector3 ab{ b.x - a.x, 0.0f, b.z - a.z };
		const Vector3 ap{ point.x - a.x, 0.0f, point.z - a.z };
		const float denom = ab.x * ab.x + ab.z * ab.z;
		if (denom <= epsilon)
		{
			return Vector3::LengthXZ(Vector3{ point.x - a.x, 0.0f, point.z - a.z });
		}

		const float t = std::clamp((ap.x * ab.x + ap.z * ab.z) / denom, 0.0f, 1.0f);
		const Vector3 closest{ a.x + ab.x * t, 0.0f, a.z + ab.z * t };
		return Vector3::LengthXZ(Vector3{ point.x - closest.x, 0.0f, point.z - closest.z });
	}
} // namespace Ken4lowEngine::MathUtil
