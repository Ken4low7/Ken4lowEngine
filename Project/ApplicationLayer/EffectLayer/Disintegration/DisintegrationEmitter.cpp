#define NOMINMAX
#include "DisintegrationEmitter.h"

#include <algorithm>
#include <cmath>

namespace
{
	K4E::Vector4 ClampColor(const K4E::Vector4& color)
	{
		return {
			std::clamp(color.x, 0.0f, 1.0f),
			std::clamp(color.y, 0.0f, 1.0f),
			std::clamp(color.z, 0.0f, 1.0f),
			std::clamp(color.w, 0.0f, 1.0f),
		};
	}

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

std::vector<DisintegrationParticle> DisintegrationEmitter::EmitFromModel(
	const K4E::ModelData& modelData,
	const K4E::Matrix4x4& worldMatrix,
	const Settings& settings)
{
	rng_.seed(settings.placementSeed ^ 0xD157E6A7u);
	ModelSurfaceSampler sampler;
	const std::vector<DisintegrationSamplePoint> samples = sampler.SampleFromModel(
		modelData,
		worldMatrix,
		settings.particleCount,
		settings.placementMode != DisintegrationPlacementMode::RandomSurface ? true : settings.surfaceSampling,
		settings.particleSize * 1.5f,
		settings.placementMode,
		settings.placementSeed,
		settings.placementSpacing,
		settings.voxelSpacing,
		settings.maxVoxelBlockCount,
		settings.voxelSurfaceThickness,
		settings.useVoxelInsideTest,
		settings.useVoxelSurfaceNearTest,
		settings.alignVoxelGridToCenter);
	return EmitFromSamples(samples, worldMatrix.GetTranslation(), settings);
}

std::vector<DisintegrationParticle> DisintegrationEmitter::EmitFromSamples(
	const std::vector<DisintegrationSamplePoint>& samples,
	const K4E::Vector3& center,
	const Settings& settings)
{
	rng_.seed(settings.placementSeed ^ 0xD157E6A7u);
	std::vector<DisintegrationParticle> particles;
	particles.reserve(samples.size());
	if (samples.empty()) { return particles; }

	const float effectiveSurfaceInset = settings.useSurfaceInset
		? std::max(settings.autoSurfaceInsetFromBlockSize ? settings.particleSize * 0.5f : settings.surfaceInset, 0.0f)
		: 0.0f;

	for (const auto& sample : samples)
	{
		const K4E::Vector3 surfaceNormal = ResolveSurfaceNormal(sample, center);
		// 表面上のサンプル点から法線の内側へ中心を入れ、Cubeの外側への半分はみ出しを抑える。
		const K4E::Vector3 insetPosition = sample.position - surfaceNormal * effectiveSurfaceInset;
		K4E::Vector3 outward = K4E::Vector3::NormalizeSafe(sample.position - center, surfaceNormal);
		outward = K4E::Vector3::NormalizeSafe(outward * 0.70f + surfaceNormal * 0.25f + RandomUnitVector() * 0.20f, surfaceNormal);

		const float brightness = RandomRange(1.0f - settings.colorVariation, 1.0f + settings.colorVariation);
		K4E::Vector4 color = ClampColor({
			settings.baseColor.x * brightness + RandomRange(-settings.colorVariation, settings.colorVariation) * 0.25f,
			settings.baseColor.y * brightness + RandomRange(-settings.colorVariation, settings.colorVariation) * 0.25f,
			settings.baseColor.z * brightness + RandomRange(-settings.colorVariation, settings.colorVariation) * 0.25f,
			settings.baseColor.w,
		});

		DisintegrationParticle particle{};
		particle.initialPosition = insetPosition;
		particle.origin = insetPosition;
		particle.position = insetPosition;
		particle.outward = outward;
		const float rotationRandomness = settings.useRandomRotation ? settings.rotationRandomness : 0.0f;
		particle.rotation = {
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
		};
		particle.rotationVelocity = {
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
			RandomRange(-rotationRandomness, rotationRandomness),
		};
		const float scaleVariation = settings.useRandomScale ? std::max(settings.scaleVariation, 0.0f) : 0.0f;
		particle.scale = {
			RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation),
			RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation),
			RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation),
		};
		particle.velocity = outward * RandomRange(settings.spreadPower * 0.15f, settings.spreadPower * 0.55f);
		particle.velocity.y += RandomRange(-settings.upwardPower * 0.25f, settings.upwardPower);
		particle.life = RandomRange(settings.lifeTime * 0.85f, settings.lifeTime * 1.20f);
		particle.startDelay = RandomRange(0.0f, settings.startDelay);
		particle.size = settings.particleSize * RandomRange(1.0f - scaleVariation, 1.0f + scaleVariation);
		particle.color = color;
		particle.edgeColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		particle.alpha = 1.0f;
		particle.edgeFactor = 0.0f;
		particle.erosionNoise = 0.5f;
		particle.sweepCoord = 0.0f;
		particle.active = true;
		particle.alive = true;
		particles.push_back(particle);
	}

	return particles;
}

float DisintegrationEmitter::Random01()
{
	return unitDist_(rng_);
}

float DisintegrationEmitter::RandomRange(float minValue, float maxValue)
{
	return minValue + (maxValue - minValue) * Random01();
}

K4E::Vector3 DisintegrationEmitter::RandomUnitVector()
{
	K4E::Vector3 v{};
	do
	{
		v = { RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f), RandomRange(-1.0f, 1.0f) };
	} while (K4E::Vector3::LengthSquared(v) <= 0.000001f);

	return K4E::Vector3::Normalize(v);
}
