#define NOMINMAX
#include "ModelSurfaceSampler.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <tuple>

namespace
{
	K4E::Vector3 ToVector3(const K4E::Vector4& v)
	{
		return { v.x, v.y, v.z };
	}

	float TriangleArea(const K4E::Vector3& a, const K4E::Vector3& b, const K4E::Vector3& c)
	{
		return K4E::Vector3::Length(K4E::Vector3::Cross(b - a, c - a)) * 0.5f;
	}

	K4E::Vector3 TransformDirection(const K4E::Vector3& direction, const K4E::Matrix4x4& matrix)
	{
		return {
			direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0],
			direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1],
			direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2],
		};
	}

	struct WorldTriangle
	{
		K4E::Vector3 a{};
		K4E::Vector3 b{};
		K4E::Vector3 c{};
		K4E::Vector3 normal{ 0.0f, 1.0f, 0.0f };
	};

	K4E::Vector3 ClosestPointOnTriangle(const K4E::Vector3& point, const WorldTriangle& tri)
	{
		const K4E::Vector3 ab = tri.b - tri.a;
		const K4E::Vector3 ac = tri.c - tri.a;
		const K4E::Vector3 ap = point - tri.a;
		const float d1 = K4E::Vector3::Dot(ab, ap);
		const float d2 = K4E::Vector3::Dot(ac, ap);
		if (d1 <= 0.0f && d2 <= 0.0f) { return tri.a; }

		const K4E::Vector3 bp = point - tri.b;
		const float d3 = K4E::Vector3::Dot(ab, bp);
		const float d4 = K4E::Vector3::Dot(ac, bp);
		if (d3 >= 0.0f && d4 <= d3) { return tri.b; }

		const float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
		{
			const float v = d1 / (d1 - d3);
			return tri.a + ab * v;
		}

		const K4E::Vector3 cp = point - tri.c;
		const float d5 = K4E::Vector3::Dot(ab, cp);
		const float d6 = K4E::Vector3::Dot(ac, cp);
		if (d6 >= 0.0f && d5 <= d6) { return tri.c; }

		const float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
		{
			const float w = d2 / (d2 - d6);
			return tri.a + ac * w;
		}

		const float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
		{
			const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return tri.b + (tri.c - tri.b) * w;
		}

		const float denom = 1.0f / (va + vb + vc);
		const float v = vb * denom;
		const float w = vc * denom;
		return tri.a + ab * v + ac * w;
	}

	bool RayIntersectsTriangle(const K4E::Vector3& origin, const K4E::Vector3& direction, const WorldTriangle& tri, float& t)
	{
		constexpr float kEpsilon = 0.000001f;
		const K4E::Vector3 edge1 = tri.b - tri.a;
		const K4E::Vector3 edge2 = tri.c - tri.a;
		const K4E::Vector3 h = K4E::Vector3::Cross(direction, edge2);
		const float det = K4E::Vector3::Dot(edge1, h);
		if (det > -kEpsilon && det < kEpsilon) { return false; }

		const float invDet = 1.0f / det;
		const K4E::Vector3 s = origin - tri.a;
		const float u = invDet * K4E::Vector3::Dot(s, h);
		if (u < 0.0f || u > 1.0f) { return false; }

		const K4E::Vector3 q = K4E::Vector3::Cross(s, edge1);
		const float v = invDet * K4E::Vector3::Dot(direction, q);
		if (v < 0.0f || u + v > 1.0f) { return false; }

		t = invDet * K4E::Vector3::Dot(edge2, q);
		return t > kEpsilon;
	}
}

std::vector<DisintegrationSamplePoint> ModelSurfaceSampler::SampleFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	int sampleCount,
	bool surfaceSampling,
	float vertexJitterRadius,
	DisintegrationPlacementMode placementMode,
	uint32_t placementSeed,
	float placementSpacing,
	float voxelSpacing,
	int maxVoxelBlockCount,
	float voxelSurfaceThickness,
	bool useVoxelInsideTest,
	bool useVoxelSurfaceNearTest,
	bool alignVoxelGridToCenter)
{
	rng_.seed(placementSeed);
	std::vector<TriangleSample> triangles;
	std::vector<VertexSample> vertices;
	triangles.reserve(1024);
	vertices.reserve(1024);

	float totalArea = 0.0f;
	for (const auto& subMesh : modelData.subMeshes)
	{
		for (const auto& vertex : subMesh.vertices)
		{
			vertices.push_back({ ToVector3(vertex.position), K4E::Vector3::NormalizeSafe(vertex.normal, { 0.0f, 1.0f, 0.0f }) });
		}

		if (subMesh.indices.size() >= 3)
		{
			for (size_t i = 0; i + 2 < subMesh.indices.size(); i += 3)
			{
				const uint32_t ia = subMesh.indices[i + 0];
				const uint32_t ib = subMesh.indices[i + 1];
				const uint32_t ic = subMesh.indices[i + 2];
				if (ia >= subMesh.vertices.size() || ib >= subMesh.vertices.size() || ic >= subMesh.vertices.size()) { continue; }

				const K4E::Vector3 a = ToVector3(subMesh.vertices[ia].position);
				const K4E::Vector3 b = ToVector3(subMesh.vertices[ib].position);
				const K4E::Vector3 c = ToVector3(subMesh.vertices[ic].position);
				const float area = TriangleArea(a, b, c);
				if (area <= 0.000001f) { continue; }

				totalArea += area;
				triangles.push_back({ a, b, c, K4E::Vector3::NormalizeSafe(K4E::Vector3::Cross(b - a, c - a), { 0.0f, 1.0f, 0.0f }), totalArea });
			}
		}
		else if (subMesh.vertices.size() >= 3)
		{
			for (size_t i = 0; i + 2 < subMesh.vertices.size(); i += 3)
			{
				const K4E::Vector3 a = ToVector3(subMesh.vertices[i + 0].position);
				const K4E::Vector3 b = ToVector3(subMesh.vertices[i + 1].position);
				const K4E::Vector3 c = ToVector3(subMesh.vertices[i + 2].position);
				const float area = TriangleArea(a, b, c);
				if (area <= 0.000001f) { continue; }

				// Boundsではなく三角形表面の面積重みによってモデル形状を復元する点を選ぶ。
				totalArea += area;
				triangles.push_back({ a, b, c, K4E::Vector3::NormalizeSafe(K4E::Vector3::Cross(b - a, c - a), { 0.0f, 1.0f, 0.0f }), totalArea });
			}
		}
	}

	std::vector<DisintegrationSamplePoint> samples;
	samples.reserve(static_cast<size_t>(std::max(0, sampleCount)));
	if ((triangles.empty() && vertices.empty()) || sampleCount <= 0) { return samples; }

	if (placementMode == DisintegrationPlacementMode::VoxelFill && !triangles.empty())
	{
		std::vector<WorldTriangle> worldTriangles;
		worldTriangles.reserve(triangles.size());
		K4E::Vector3 boundsMin{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
		K4E::Vector3 boundsMax{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };
		for (const auto& tri : triangles)
		{
			WorldTriangle worldTri{};
			worldTri.a = K4E::Vector3::Transform(tri.a, worldMatrix);
			worldTri.b = K4E::Vector3::Transform(tri.b, worldMatrix);
			worldTri.c = K4E::Vector3::Transform(tri.c, worldMatrix);
			worldTri.normal = K4E::Vector3::NormalizeSafe(TransformDirection(tri.normal, worldMatrix), { 0.0f, 1.0f, 0.0f });
			worldTriangles.push_back(worldTri);
			for (const auto& p : { worldTri.a, worldTri.b, worldTri.c })
			{
				boundsMin.x = std::min(boundsMin.x, p.x);
				boundsMin.y = std::min(boundsMin.y, p.y);
				boundsMin.z = std::min(boundsMin.z, p.z);
				boundsMax.x = std::max(boundsMax.x, p.x);
				boundsMax.y = std::max(boundsMax.y, p.y);
				boundsMax.z = std::max(boundsMax.z, p.z);
			}
		}

		const int maxBlocks = std::max(maxVoxelBlockCount > 0 ? maxVoxelBlockCount : sampleCount, 1);
		const float safeSpacing = voxelSpacing > 0.0001f ? voxelSpacing : (placementSpacing > 0.0001f ? placementSpacing : std::max(vertexJitterRadius / 1.5f, 0.01f));
		const float thickness = voxelSurfaceThickness > 0.0001f ? voxelSurfaceThickness : safeSpacing * 1.5f;
		const K4E::Vector3 center = (boundsMin + boundsMax) * 0.5f;
		K4E::Vector3 start = boundsMin;
		if (alignVoxelGridToCenter)
		{
			const auto alignAxis = [safeSpacing](float minValue, float maxValue) {
				const float centerValue = (minValue + maxValue) * 0.5f;
				const int halfSteps = static_cast<int>(std::floor((centerValue - minValue) / safeSpacing));
				return centerValue - static_cast<float>(halfSteps) * safeSpacing;
			};
			start = { alignAxis(boundsMin.x, boundsMax.x), alignAxis(boundsMin.y, boundsMax.y), alignAxis(boundsMin.z, boundsMax.z) };
		}

		std::vector<DisintegrationSamplePoint> voxelCandidates;
		voxelCandidates.reserve(static_cast<size_t>(maxBlocks));
		const K4E::Vector3 rayDirection{ 1.0f, 0.173f, 0.097f };
		for (float z = start.z; z <= boundsMax.z + safeSpacing * 0.25f; z += safeSpacing)
		{
			if (z < boundsMin.z - safeSpacing * 0.25f) { continue; }
			for (float y = start.y; y <= boundsMax.y + safeSpacing * 0.25f; y += safeSpacing)
			{
				if (y < boundsMin.y - safeSpacing * 0.25f) { continue; }
				for (float x = start.x; x <= boundsMax.x + safeSpacing * 0.25f; x += safeSpacing)
				{
					if (x < boundsMin.x - safeSpacing * 0.25f) { continue; }
					const K4E::Vector3 point{ x, y, z };
					bool inside = false;
					if (useVoxelInsideTest)
					{
						int hitCount = 0;
						for (const auto& tri : worldTriangles)
						{
							float t = 0.0f;
							if (RayIntersectsTriangle(point, rayDirection, tri, t)) { ++hitCount; }
						}
						inside = (hitCount & 1) != 0;
					}

					float nearestDistanceSq = std::numeric_limits<float>::max();
					K4E::Vector3 nearestNormal{ 0.0f, 1.0f, 0.0f };
					if (!inside || useVoxelSurfaceNearTest)
					{
						for (const auto& tri : worldTriangles)
						{
							const K4E::Vector3 closest = ClosestPointOnTriangle(point, tri);
							const float distanceSq = K4E::Vector3::LengthSquared(point - closest);
							if (distanceSq < nearestDistanceSq)
							{
								nearestDistanceSq = distanceSq;
								nearestNormal = tri.normal;
							}
						}
					}

					const bool nearSurface = useVoxelSurfaceNearTest && nearestDistanceSq <= thickness * thickness;
					if ((useVoxelInsideTest && inside) || nearSurface)
					{
						DisintegrationSamplePoint sample{};
						// VoxelFillはモデルAABB内の格子中心を形状判定で間引き、箱埋めではない塊を作る。
						sample.position = point;
						sample.normal = K4E::Vector3::NormalizeSafe(nearestNormal, K4E::Vector3::NormalizeSafe(point - center, { 0.0f, 1.0f, 0.0f }));
						voxelCandidates.push_back(sample);
					}
				}
			}
		}

		if (!voxelCandidates.empty())
		{
			const int outputCount = std::min(maxBlocks, static_cast<int>(voxelCandidates.size()));
			samples.reserve(static_cast<size_t>(outputCount));
			for (int i = 0; i < outputCount; ++i)
			{
				const size_t index = outputCount < static_cast<int>(voxelCandidates.size())
					? (static_cast<size_t>(i) * voxelCandidates.size()) / static_cast<size_t>(outputCount)
					: static_cast<size_t>(i);
				samples.push_back(voxelCandidates[std::min(index, voxelCandidates.size() - 1)]);
			}
			return samples;
		}
	}

	const bool useUniformSurface = placementMode == DisintegrationPlacementMode::UniformSurface && !triangles.empty();
	const bool useAlignedSurfaceGrid = placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid && !triangles.empty();
	const bool useTriangleSurface = (surfaceSampling || useUniformSurface || useAlignedSurfaceGrid) && !triangles.empty();
	if (useAlignedSurfaceGrid)
	{
		std::vector<DisintegrationSamplePoint> gridCandidates;
		const float safeSpacing = placementSpacing > 0.0001f ? placementSpacing : std::sqrt(totalArea / static_cast<float>(sampleCount));
		for (size_t triIndex = 0; triIndex < triangles.size(); ++triIndex)
		{
			const auto& tri = triangles[triIndex];
			const float previousArea = triIndex == 0 ? 0.0f : triangles[triIndex - 1].cumulativeArea;
			const float triangleArea = std::max(tri.cumulativeArea - previousArea, 0.0f);
			const int subdivision = std::clamp(static_cast<int>(std::ceil(std::sqrt(triangleArea) / safeSpacing * 2.0f)), 1, 128);
			for (int y = 0; y <= subdivision; ++y)
			{
				for (int x = 0; x + y <= subdivision; ++x)
				{
					const float baryB = (static_cast<float>(x) + 0.5f) / (static_cast<float>(subdivision) + 1.0f);
					const float baryC = (static_cast<float>(y) + 0.5f) / (static_cast<float>(subdivision) + 1.0f);
					if (baryB + baryC >= 1.0f) { continue; }
					const K4E::Vector3 local = tri.a + (tri.b - tri.a) * baryB + (tri.c - tri.a) * baryC;
					DisintegrationSamplePoint sample{};
					sample.position = K4E::Vector3::Transform(local, worldMatrix);
					sample.normal = K4E::Vector3::NormalizeSafe(TransformDirection(tri.normal, worldMatrix), { 0.0f, 1.0f, 0.0f });
					gridCandidates.push_back(sample);
				}
			}
		}

		std::sort(gridCandidates.begin(), gridCandidates.end(), [](const DisintegrationSamplePoint& a, const DisintegrationSamplePoint& b) {
			return std::tie(a.position.x, a.position.y, a.position.z) < std::tie(b.position.x, b.position.y, b.position.z);
		});
		if (!gridCandidates.empty())
		{
			for (int i = 0; i < sampleCount; ++i)
			{
				const size_t index = sampleCount <= static_cast<int>(gridCandidates.size())
					? (static_cast<size_t>(i) * gridCandidates.size()) / static_cast<size_t>(sampleCount)
					: static_cast<size_t>(i) % gridCandidates.size();
				samples.push_back(gridCandidates[std::min(index, gridCandidates.size() - 1)]);
			}
			return samples;
		}
	}

	for (int i = 0; i < sampleCount; ++i)
	{
		K4E::Vector3 local{};
		K4E::Vector3 localNormal{ 0.0f, 1.0f, 0.0f };

		if (useTriangleSurface)
		{
			const float areaPick = useUniformSurface
				? totalArea * (static_cast<float>(i) + 0.5f) / static_cast<float>(sampleCount)
				: RandomRange(0.0f, totalArea);
			auto it = std::lower_bound(
				triangles.begin(), triangles.end(), areaPick,
				[](const TriangleSample& tri, float value) { return tri.cumulativeArea < value; });
			if (it == triangles.end()) { it = triangles.end() - 1; }

			float u = Random01();
			float v = Random01();
			if (useUniformSurface)
			{
				// 面積で層化した三角形上に低差異点を置き、毎回同じ均一表面配置にする。
				u = (static_cast<float>((static_cast<uint32_t>(i) * 2654435761u + placementSeed) & 0x00FFFFFFu) + 0.5f) / static_cast<float>(0x01000000u);
				v = VanDerCorput(static_cast<uint32_t>(i) + (placementSeed | 1u));
				const float sqrtU = std::sqrt(u);
				const float baryB = sqrtU * (1.0f - v);
				const float baryC = sqrtU * v;
				local = it->a + (it->b - it->a) * baryB + (it->c - it->a) * baryC;
			}
			else
			{
				if (u + v > 1.0f)
				{
					u = 1.0f - u;
					v = 1.0f - v;
				}
				local = it->a + (it->b - it->a) * u + (it->c - it->a) * v;
			}
			localNormal = it->normal;
		}
		else
		{
			const bool useUniformVertexFallback = placementMode == DisintegrationPlacementMode::UniformSurface || placementMode == DisintegrationPlacementMode::AlignedSurfaceGrid;
			const size_t sampleIndex = useUniformVertexFallback
				? (static_cast<size_t>(i) * vertices.size()) / static_cast<size_t>(sampleCount)
				: std::min(static_cast<size_t>(Random01() * static_cast<float>(vertices.size())), vertices.size() - 1);
			const VertexSample& sample = vertices[std::min(sampleIndex, vertices.size() - 1)];
			local = useUniformVertexFallback
				? sample.position
				: sample.position + RandomUnitVector() * RandomRange(0.0f, vertexJitterRadius);
			localNormal = sample.normal;
		}

		DisintegrationSamplePoint sample{};
		sample.position = K4E::Vector3::Transform(local, worldMatrix);
		sample.normal = K4E::Vector3::NormalizeSafe(TransformDirection(localNormal, worldMatrix), { 0.0f, 1.0f, 0.0f });
		samples.push_back(sample);
	}

	return samples;
}

float ModelSurfaceSampler::Random01()
{
	return unitDist_(rng_);
}

float ModelSurfaceSampler::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random01();
}

K4E::Vector3 ModelSurfaceSampler::RandomUnitVector()
{
	K4E::Vector3 v{};
	do
	{
		v = { RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f) };
	} while (K4E::Vector3::LengthSquared(v) <= 0.000001f);

	return K4E::Vector3::Normalize(v);
}


float ModelSurfaceSampler::VanDerCorput(uint32_t value) const
{
	value = (value << 16u) | (value >> 16u);
	value = ((value & 0x55555555u) << 1u) | ((value & 0xAAAAAAAAu) >> 1u);
	value = ((value & 0x33333333u) << 2u) | ((value & 0xCCCCCCCCu) >> 2u);
	value = ((value & 0x0F0F0F0Fu) << 4u) | ((value & 0xF0F0F0F0u) >> 4u);
	value = ((value & 0x00FF00FFu) << 8u) | ((value & 0xFF00FF00u) >> 8u);
	return static_cast<float>(value) * 2.3283064365386963e-10f;
}
