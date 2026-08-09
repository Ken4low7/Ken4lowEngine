#pragma once
#include "ModelSurfaceSampler.h"
#include "ReconstructionBlock.h"
#include "Matrix4x4.h"
#include "ModelData.h"
#include "Vector4.h"

#include <cstdint>
#include <random>
#include <vector>

/// -------------------------------------------------------------
/// モデル表面サンプルを目標位置にした再構築ブロックを生成する
/// -------------------------------------------------------------
class ReconstructionEmitter
{
public:
	struct Settings
	{
		int blockCount = 1200;
		float blockSize = 0.045f;
		float startScatterRadius = 3.0f;
		float startHeight = 2.5f;
		float startDelayRange = 0.45f;
		float rotationRandomness = 4.0f;
		DisintegrationPlacementMode placementMode = DisintegrationPlacementMode::RandomSurface;
		bool useRandomScale = true;
		float scaleVariation = 0.12f;
		bool useRandomRotation = true;
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
		K4E::Vector4 color{ 0.64f, 0.72f, 0.86f, 1.0f };
		float colorVariation = 0.18f;
		bool useFadeIn = true;
		float fadeInDuration = 0.4f;
		float fadeInDelayRange = 0.2f;
		float initialAlpha = 0.0f;
		float targetAlpha = 1.0f;
	};

	std::vector<ReconstructionBlock> EmitFromModel(
		const K4E::ModelData& modelData,
		const K4E::Matrix4x4& worldMatrix,
		const Settings& settings);
	std::vector<ReconstructionBlock> EmitFromSamples(
		const std::vector<DisintegrationSamplePoint>& targets,
		const K4E::Vector3& center,
		const Settings& settings);

private:
	float Random01();
	float RandomRange(float minValue, float maxValue);
	K4E::Vector3 RandomUnitVector();
	K4E::Vector4 MakeColor(const Settings& settings);

	std::mt19937 rng_{ 0xC0DE2026u };
	std::uniform_real_distribution<float> unitDist_{ 0.0f, 1.0f };
};
