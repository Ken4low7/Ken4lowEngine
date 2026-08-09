#pragma once
#include "Matrix4x4.h"
#include "ModelData.h"
#include "Vector3.h"

#include <cstdint>
#include <random>
#include <vector>

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// 崩壊ブロックの初期配置モード
/// -------------------------------------------------------------
enum class DisintegrationPlacementMode
{
	RandomSurface,
	UniformSurface,
	AlignedSurfaceGrid,
	VoxelFill,
};

/// -------------------------------------------------------------
/// 崩壊・再構築エフェクトで共有するモデル表面サンプル点
/// -------------------------------------------------------------
struct DisintegrationSamplePoint
{
	K4E::Vector3 position{};
	K4E::Vector3 normal{ 0.0f, 1.0f, 0.0f };
};

/// -------------------------------------------------------------
/// AABBグリッドではなく三角形表面または頂点から形状点を生成するサンプラー
/// -------------------------------------------------------------
class ModelSurfaceSampler
{
public:
	std::vector<DisintegrationSamplePoint> SampleFromModel(
		const K4E::ModelData& modelData,
		const K4E::Matrix4x4& worldMatrix,
		int sampleCount,
		bool surfaceSampling,
		float vertexJitterRadius = 0.0f,
		DisintegrationPlacementMode placementMode = DisintegrationPlacementMode::RandomSurface,
		uint32_t placementSeed = 0x51A7F00Du,
		float placementSpacing = 0.0f,
		float voxelSpacing = 0.0f,
		int maxVoxelBlockCount = 10000,
		float voxelSurfaceThickness = 0.0f,
		bool useVoxelInsideTest = true,
		bool useVoxelSurfaceNearTest = true,
		bool alignVoxelGridToCenter = true);

private:
	struct TriangleSample
	{
		K4E::Vector3 a{};
		K4E::Vector3 b{};
		K4E::Vector3 c{};
		K4E::Vector3 normal{ 0.0f, 1.0f, 0.0f };
		float cumulativeArea = 0.0f;
	};

	struct VertexSample
	{
		K4E::Vector3 position{};
		K4E::Vector3 normal{ 0.0f, 1.0f, 0.0f };
	};

	float Random01();
	float RandomRange(float minValue, float maxValue);
	K4E::Vector3 RandomUnitVector();
	float VanDerCorput(uint32_t value) const;

	std::mt19937 rng_{ 0x51A7F00Du };
	std::uniform_real_distribution<float> unitDist_{ 0.0f, 1.0f };
};
