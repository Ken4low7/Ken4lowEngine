#pragma once

#include "BoundingVolume.h"
#include "NormalCone.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <vector>

namespace Ken4lowEngine
{
	struct VisibilityMeshletBuildSettings
	{
		uint32_t maxVertices = 64;
		uint32_t maxTriangles = 126;
	};

	/// <summary>
	/// 既存Index Bufferの連続Rangeを壊さずに保持する、Visibility判定用のCPU Meshletメタデータです。
	/// </summary>
	struct VisibilityMeshlet
	{
		uint32_t startIndex = 0;
		uint32_t indexCount = 0;
		uint32_t triangleCount = 0;
		uint32_t uniqueVertexCount = 0;
		BoundingSphere bounds{};
		NormalCone normalCone{};
	};

	inline BoundingSphere BuildMeshletBounds(
		const std::vector<VertexData>& vertices,
		const std::vector<uint32_t>& indices,
		uint32_t startIndex,
		uint32_t indexCount)
	{
		BoundingSphere bounds{};
		if (indexCount == 0 || startIndex >= indices.size()) { return bounds; }

		Vector3 minPosition{
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max(),
			std::numeric_limits<float>::max()
		};
		Vector3 maxPosition{
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest(),
			std::numeric_limits<float>::lowest()
		};
		bool hasVertex = false;
		const uint32_t endIndex = std::min<uint32_t>(startIndex + indexCount, static_cast<uint32_t>(indices.size()));
		for (uint32_t index = startIndex; index < endIndex; ++index)
		{
			const uint32_t vertexIndex = indices[index];
			if (vertexIndex >= vertices.size()) { continue; }
			const Vector3 position{ vertices[vertexIndex].position.x, vertices[vertexIndex].position.y, vertices[vertexIndex].position.z };
			minPosition.x = std::min(minPosition.x, position.x);
			minPosition.y = std::min(minPosition.y, position.y);
			minPosition.z = std::min(minPosition.z, position.z);
			maxPosition.x = std::max(maxPosition.x, position.x);
			maxPosition.y = std::max(maxPosition.y, position.y);
			maxPosition.z = std::max(maxPosition.z, position.z);
			hasVertex = true;
		}
		if (!hasVertex) { return bounds; }

		bounds.center = (minPosition + maxPosition) * 0.5f;
		float radius = 0.0f;
		for (uint32_t index = startIndex; index < endIndex; ++index)
		{
			const uint32_t vertexIndex = indices[index];
			if (vertexIndex >= vertices.size()) { continue; }
			const Vector3 position{ vertices[vertexIndex].position.x, vertices[vertexIndex].position.y, vertices[vertexIndex].position.z };
			radius = std::max(radius, Vector3::Length(position - bounds.center));
		}
		bounds.radius = radius;
		return bounds;
	}

	inline std::vector<VisibilityMeshlet> BuildVisibilityMeshlets(
		const std::vector<VertexData>& vertices,
		const std::vector<uint32_t>& indices,
		VisibilityMeshletBuildSettings settings = {})
	{
		std::vector<VisibilityMeshlet> meshlets;
		if (vertices.empty() || indices.size() < 3) { return meshlets; }

		settings.maxVertices = std::max(settings.maxVertices, 3u);
		settings.maxTriangles = std::max(settings.maxTriangles, 1u);
		std::unordered_set<uint32_t> uniqueVertices;
		uint32_t meshletStartIndex = 0;
		uint32_t meshletIndexCount = 0;

		auto flushMeshlet = [&]()
		{
			if (meshletIndexCount == 0) { return; }
			VisibilityMeshlet meshlet{};
			meshlet.startIndex = meshletStartIndex;
			meshlet.indexCount = meshletIndexCount;
			meshlet.triangleCount = meshletIndexCount / 3u;
			meshlet.uniqueVertexCount = static_cast<uint32_t>(uniqueVertices.size());
			meshlet.bounds = BuildMeshletBounds(vertices, indices, meshletStartIndex, meshletIndexCount);
			std::vector<uint32_t> localRangeIndices(
				indices.begin() + meshletStartIndex,
				indices.begin() + meshletStartIndex + meshletIndexCount);
			meshlet.normalCone = BuildNormalCone(vertices, localRangeIndices);
			meshlets.push_back(meshlet);
			uniqueVertices.clear();
			meshletIndexCount = 0;
		};

		const uint32_t triangleIndexCount = static_cast<uint32_t>(indices.size() - (indices.size() % 3));
		for (uint32_t start = 0; start < triangleIndexCount; start += 3)
		{
			uint32_t newUniqueVertices = 0;
			for (uint32_t corner = 0; corner < 3; ++corner)
			{
				if (uniqueVertices.find(indices[start + corner]) == uniqueVertices.end()) { ++newUniqueVertices; }
			}

			const uint32_t currentTriangles = meshletIndexCount / 3u;
			const bool exceedsTriangleBudget = currentTriangles >= settings.maxTriangles;
			const bool exceedsVertexBudget = !uniqueVertices.empty()
				&& uniqueVertices.size() + newUniqueVertices > settings.maxVertices;
			if (exceedsTriangleBudget || exceedsVertexBudget)
			{
				flushMeshlet();
				meshletStartIndex = start;
			}

			if (meshletIndexCount == 0) { meshletStartIndex = start; }
			uniqueVertices.insert(indices[start + 0]);
			uniqueVertices.insert(indices[start + 1]);
			uniqueVertices.insert(indices[start + 2]);
			meshletIndexCount += 3;
		}
		flushMeshlet();
		return meshlets; // 既存Draw rangeを保持したまま、CPU reference用Visibility clusterだけを生成する。
	}
} // namespace Ken4lowEngine
