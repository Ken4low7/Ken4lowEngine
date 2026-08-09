#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// <summary>
	/// Frustum Culling で使う簡易的な球 Bounds。
	/// </summary>
	struct BoundingSphere
	{
		Vector3 center{};
		float radius = 1.0f;
	};

	/// <summary>
	/// Frustum Culling で使う軸平行 AABB Bounds。
	/// </summary>
	struct BoundingAABB
	{
		Vector3 min{};
		Vector3 max{};
	};
}
