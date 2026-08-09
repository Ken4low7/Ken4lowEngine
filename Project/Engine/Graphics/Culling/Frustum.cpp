#include "Frustum.h"

#include <cmath>

namespace Ken4lowEngine
{
	void Frustum::BuildFromViewProjection(const Matrix4x4& viewProjection)
	{
		// 行ベクトル前提のクリップ空間条件から、ワールド空間の6平面を抽出する。
		planes_[0] = { viewProjection.m[0][3] + viewProjection.m[0][0], viewProjection.m[1][3] + viewProjection.m[1][0], viewProjection.m[2][3] + viewProjection.m[2][0], viewProjection.m[3][3] + viewProjection.m[3][0] }; // Left
		planes_[1] = { viewProjection.m[0][3] - viewProjection.m[0][0], viewProjection.m[1][3] - viewProjection.m[1][0], viewProjection.m[2][3] - viewProjection.m[2][0], viewProjection.m[3][3] - viewProjection.m[3][0] }; // Right
		planes_[2] = { viewProjection.m[0][3] + viewProjection.m[0][1], viewProjection.m[1][3] + viewProjection.m[1][1], viewProjection.m[2][3] + viewProjection.m[2][1], viewProjection.m[3][3] + viewProjection.m[3][1] }; // Bottom
		planes_[3] = { viewProjection.m[0][3] - viewProjection.m[0][1], viewProjection.m[1][3] - viewProjection.m[1][1], viewProjection.m[2][3] - viewProjection.m[2][1], viewProjection.m[3][3] - viewProjection.m[3][1] }; // Top
		planes_[4] = { viewProjection.m[0][2], viewProjection.m[1][2], viewProjection.m[2][2], viewProjection.m[3][2] }; // Near
		planes_[5] = { viewProjection.m[0][3] - viewProjection.m[0][2], viewProjection.m[1][3] - viewProjection.m[1][2], viewProjection.m[2][3] - viewProjection.m[2][2], viewProjection.m[3][3] - viewProjection.m[3][2] }; // Far

		for (auto& plane : planes_)
		{
			NormalizePlane(plane);
		}
	}

	bool Frustum::Intersects(const BoundingSphere& sphere) const
	{
		for (const auto& plane : planes_)
		{
			const float distance = plane.x * sphere.center.x + plane.y * sphere.center.y + plane.z * sphere.center.z + plane.w;
			if (distance < -sphere.radius)
			{
				return false;
			}
		}

		return true;
	}

	bool Frustum::Intersects(const BoundingAABB& aabb) const
	{
		for (const auto& plane : planes_)
		{
			const Vector3 positiveVertex = {
				plane.x >= 0.0f ? aabb.max.x : aabb.min.x,
				plane.y >= 0.0f ? aabb.max.y : aabb.min.y,
				plane.z >= 0.0f ? aabb.max.z : aabb.min.z,
			};

			const float distance = plane.x * positiveVertex.x + plane.y * positiveVertex.y + plane.z * positiveVertex.z + plane.w;
			if (distance < 0.0f)
			{
				return false;
			}
		}

		return true;
	}

	void Frustum::NormalizePlane(Vector4& plane) const
	{
		const float length = std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
		if (length <= 0.000001f)
		{
			return;
		}

		plane.x /= length;
		plane.y /= length;
		plane.z /= length;
		plane.w /= length;
	}
}
