#pragma once

#include "Vector3.h"
#include "VertexData.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Triangle normal群を包むCPU側Normal Coneメタデータです。
	/// Phase 15.1では解析/統計だけに使い、Runtime rejectionはMeshlet化後に有効化します。
	/// </summary>
	struct NormalCone
	{
		Vector3 axis{ 0.0f, 0.0f, 1.0f };
		float minDot = -1.0f;
		uint32_t triangleCount = 0;
		uint32_t degenerateTriangleCount = 0;
		bool valid = false;

		bool IsBackfaceCullCandidate() const
		{
			// 半角が90度未満なら、将来のMeshlet Normal Cone判定へ利用できる候補とする。
			return valid && triangleCount > 0 && minDot > 0.0f;
		}

		float GetHalfAngleRadians() const
		{
			return valid ? std::acos(std::clamp(minDot, -1.0f, 1.0f)) : 3.14159265358979323846f;
		}
	};

	inline NormalCone BuildNormalCone(const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices)
	{
		NormalCone cone{};
		if (vertices.empty() || indices.size() < 3) { return cone; }

		std::vector<Vector3> triangleNormals;
		triangleNormals.reserve(indices.size() / 3);
		Vector3 axisSum{};

		for (size_t index = 0; index + 2 < indices.size(); index += 3)
		{
			const uint32_t i0 = indices[index + 0];
			const uint32_t i1 = indices[index + 1];
			const uint32_t i2 = indices[index + 2];
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size()) { continue; }

			const Vector3 p0{ vertices[i0].position.x, vertices[i0].position.y, vertices[i0].position.z };
			const Vector3 p1{ vertices[i1].position.x, vertices[i1].position.y, vertices[i1].position.z };
			const Vector3 p2{ vertices[i2].position.x, vertices[i2].position.y, vertices[i2].position.z };
			const Vector3 cross = Vector3::Cross(p1 - p0, p2 - p0);
			const float lengthSquared = Vector3::LengthSquared(cross);
			if (lengthSquared <= 1.0e-12f)
			{
				++cone.degenerateTriangleCount;
				continue;
			}

			const Vector3 normal = cross / std::sqrt(lengthSquared);
			triangleNormals.push_back(normal);
			axisSum += normal;
		}

		cone.triangleCount = static_cast<uint32_t>(triangleNormals.size());
		if (triangleNormals.empty()) { return cone; }

		if (Vector3::LengthSquared(axisSum) <= 1.0e-12f)
		{
			cone.axis = triangleNormals.front(); // 相反する面が多い場合でもCone幅を正しく「広い」と判定できる軸を残す。
		}
		else
		{
			cone.axis = Vector3::Normalize(axisSum);
		}

		cone.minDot = 1.0f;
		for (const Vector3& normal : triangleNormals)
		{
			cone.minDot = (std::min)(cone.minDot, Vector3::Dot(cone.axis, normal)); // Windows.h の min マクロ展開を避ける。
		}
		cone.minDot = std::clamp(cone.minDot, -1.0f, 1.0f);
		cone.valid = true;
		return cone;
	}
} // namespace Ken4lowEngine
