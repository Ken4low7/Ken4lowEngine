#define NOMINMAX
#include "ReconstructionEmitter.h"

#include <algorithm>
#include <cmath>

namespace
{
	K4E::Vector3 ResolveSurfaceNormal(const DisintegrationSamplePoint& sample, const K4E::Vector3& center)
	{
		const K4E::Vector3 radial = K4E::Vector3::NormalizeSafe(sample.position - center, { 0.0f, 1.0f, 0.0f });
		K4E::Vector3 normal = K4E::Vector3::NormalizeSafe(sample.normal, radial);
		if (K4E::Vector3::Dot(normal, radial) < 0.0f)
		{
			normal = normal * -1.0f;
		}
		return normal;
	}
}

std::vector<ReconstructionBlock> ReconstructionEmitter::EmitFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	const Settings& settings)
{
	rng_.seed(settings.placementSeed ^ 0xC0DE2026u);
	ModelSurfaceSampler sampler;
	const std::vector<DisintegrationSamplePoint> targets = sampler.SampleFromModel(
		modelData,
		worldMatrix,
		settings.blockCount,
		settings.placementMode != DisintegrationPlacementMode::RandomSurface ? true : settings.surfaceSampling,
		settings.blockSize * 1.5f,
		settings.placementMode,
		settings.placementSeed,
		settings.placementSpacing,
		settings.voxelSpacing,
		settings.maxVoxelBlockCount,
		settings.voxelSurfaceThickness,
		settings.useVoxelInsideTest,
		settings.useVoxelSurfaceNearTest,
		settings.alignVoxelGridToCenter);

	return EmitFromSamples(targets, worldMatrix.GetTranslation(), settings);
}

std::vector<ReconstructionBlock> ReconstructionEmitter::EmitFromSamples(
	const std::vector<DisintegrationSamplePoint>& targets,
	const K4E::Vector3& center,
	const Settings& settings)
{
	rng_.seed(settings.placementSeed ^ 0xC0DE2026u);
	std::vector<ReconstructionBlock> blocks;
	blocks.reserve(targets.size());
	if (targets.empty()) { return blocks; }

	const float effectiveSurfaceInset = settings.useSurfaceInset
		? std::max(settings.autoSurfaceInsetFromBlockSize ? settings.blockSize * 0.5f : settings.surfaceInset, 0.0f)
		: 0.0f;
	for (const auto& target : targets)
	{
		const K4E::Vector3 surfaceNormal = ResolveSurfaceNormal(target, center);
		const K4E::Vector3 targetPosition = target.position - surfaceNormal * effectiveSurfaceInset;
		const K4E::Vector3 radial = K4E::Vector3::NormalizeSafe(target.position - center, RandomUnitVector());
		K4E::Vector3 scatter = radial * RandomRange(settings.startScatterRadius * 0.35f, settings.startScatterRadius);
		scatter += RandomUnitVector() * RandomRange(0.0f, settings.startScatterRadius * 0.35f);
		scatter.y += RandomRange(-0.25f, settings.startHeight);

		ReconstructionBlock block{};
		block.startPosition = targetPosition + scatter;
		block.targetPosition = targetPosition;
		block.targetNormal = surfaceNormal;
		block.position = block.startPosition;
		const float rotationRandomness = settings.useRandomRotation ? settings.rotationRandomness : 0.0f;
		block.startRotation = {
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
		};
		block.rotation = block.startRotation;
		block.rotationVelocity = {
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
		};
		const float scaleVariation = settings.useRandomScale ? std::max(settings.scaleVariation, 0.0f) : 0.0f;
		block.scale = {
			RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation),
			RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation),
			RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation),
		};
		block.color = MakeColor(settings);
		block.startDelay = RandomRange(0.0f, settings.startDelayRange);
		block.size = settings.blockSize * RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation);
		block.fadeInDelay = settings.useFadeIn ? RandomRange(0.0f, settings.fadeInDelayRange) : 0.0f;
		block.fadeInDuration = std::max(settings.fadeInDuration, 0.0001f);
		block.appearAge = 0.0f;
		block.startDistance = K4E::Vector3::Length(block.targetPosition - block.startPosition);
		block.alpha = settings.useFadeIn ? std::clamp(settings.initialAlpha, 0.0f, 1.0f) : std::clamp(settings.targetAlpha, 0.0f, 1.0f);
		block.visibility = block.alpha;
		block.alive = true;
		blocks.push_back(block);
	}

	return blocks;
}

float ReconstructionEmitter::Random01()
{
	return unitDist_(rng_);
}

float ReconstructionEmitter::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random01();
}

K4E::Vector3 ReconstructionEmitter::RandomUnitVector()
{
	K4E::Vector3 v{};
	do
	{
		v = { RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f) };
	} while (K4E::Vector3::LengthSquared(v) <= 0.000001f);

	return K4E::Vector3::Normalize(v);
}

K4E::Vector4 ReconstructionEmitter::MakeColor(const Settings& settings)
{
	const float variation = settings.colorVariation;
	const float brightness = RandomRange(1.0f - variation, 1.0f + variation);
	return {
		std::clamp(settings.color.x * brightness + RandomRange(-variation, variation) * 0.25f, 0.0f, 1.0f),
		std::clamp(settings.color.y * brightness + RandomRange(-variation, variation) * 0.25f, 0.0f, 1.0f),
		std::clamp(settings.color.z * brightness + RandomRange(-variation, variation) * 0.25f, 0.0f, 1.0f),
		std::clamp(settings.color.w, 0.0f, 1.0f),
	};
}
