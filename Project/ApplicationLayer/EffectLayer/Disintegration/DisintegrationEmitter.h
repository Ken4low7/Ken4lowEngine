#pragma once
#include "DisintegrationParticle.h"
#include "ModelSurfaceSampler.h"
#include "Matrix4x4.h"
#include "ModelData.h"
#include "Vector4.h"

#include <cstdint>
#include <random>
#include <vector>

/// -------------------------------------------------------------
/// モデル表面から崩壊粒子の初期配置を作る専用エミッター
/// -------------------------------------------------------------
class DisintegrationEmitter
{
public:
	struct Settings
	{
		int particleCount = 1200;
		float particleSize = 0.045f;
		float blockRotationRandomness = 1.2f;
		DisintegrationPlacementMode placementMode = DisintegrationPlacementMode::RandomSurface;
		bool useRandomScale = true;
		float scaleVariation = 0.18f;
		bool useRandomRotation = true;
		float rotationRandomness = 1.2f;
		uint32_t placementSeed = 0xD157E6A7u;
		float placementSpacing = 0.0f;
		float voxelSpacing = 0.0f;
		int maxVoxelBlockCount = 10000;
		float voxelSurfaceThickness = 0.0675f;
		bool useVoxelInsideTest = true;
		bool useVoxelSurfaceNearTest = true;
		bool alignVoxelGridToCenter = true;
		bool surfaceSampling = true;
		bool useSurfaceInset = false;
		float surfaceInset = 0.0225f;
		bool autoSurfaceInsetFromBlockSize = true;
		float lifeTime = 2.0f;
		float spreadPower = 1.6f;
		float upwardPower = 0.65f;
		float startDelay = 0.20f;
		K4E::Vector4 baseColor{ 0.64f, 0.61f, 0.56f, 1.0f };
		float colorVariation = 0.16f;
	};

	std::vector<DisintegrationParticle> EmitFromModel(
		const K4E::ModelData& modelData,
		const K4E::Matrix4x4& worldMatrix,
		const Settings& settings);
	std::vector<DisintegrationParticle> EmitFromSamples(
		const std::vector<DisintegrationSamplePoint>& samples,
		const K4E::Vector3& center,
		const Settings& settings);

private:
	float Random01();
	float RandomRange(float minValue, float maxValue);
	K4E::Vector3 RandomUnitVector();

	std::mt19937 rng_{ 0xD157E6A7u };
	std::uniform_real_distribution<float> unitDist_{ 0.0f, 1.0f };
};
