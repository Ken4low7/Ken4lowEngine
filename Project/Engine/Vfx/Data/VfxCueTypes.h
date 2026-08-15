#pragma once

#include "Vector3.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace Ken4lowEngine
{

enum class VfxCueTrackType : uint8_t
{
	Particle = 0,
	Fluid2D,
	VolumetricFluid,
	Light,
	PostEffect,
	CameraShake,
};

struct VfxParticleTrackPayload
{
	std::string effectAssetPath;
	std::string effectName;
	bool loop = false;
};

struct VfxFluidTrackPayload
{
	Vector3 localVelocity{ 0.0f, 1.0f, 0.0f };
	float radius = 0.5f;
	float velocityStrength = 1.0f;
	float densityRate = 1.0f;
	float temperatureRate = 1.0f;
	float falloffExponent = 2.0f;
};

struct VfxLightTrackPayload
{
	Vector3 color{ 1.0f, 1.0f, 1.0f };
	float intensity = 1.0f;
	float range = 5.0f;
};

struct VfxPostEffectTrackPayload
{
	std::string effectName;
	float weight = 1.0f;
};

struct VfxCameraShakeTrackPayload
{
	Vector3 translationAmplitude{ 0.05f, 0.05f, 0.05f };
	Vector3 rotationAmplitudeDegrees{ 0.5f, 0.5f, 0.5f };
	float frequency = 18.0f;
	float fovAmplitudeDegrees = 0.0f;
};

using VfxCueTrackPayload = std::variant<
	VfxParticleTrackPayload,
	VfxFluidTrackPayload,
	VfxLightTrackPayload,
	VfxPostEffectTrackPayload,
	VfxCameraShakeTrackPayload>;

struct VfxCueTrackDesc
{
	std::string name;
	VfxCueTrackType type = VfxCueTrackType::Particle;
	bool enabled = true;
	float startTime = 0.0f;
	float duration = 0.0f;
	Vector3 localOffset{};
	VfxCueTrackPayload payload = VfxParticleTrackPayload{};
};

struct VfxCueDesc
{
	static constexpr uint32_t kCurrentSchemaVersion = 1;
	static constexpr uint32_t kMaxTracks = 256;

	uint32_t schemaVersion = kCurrentSchemaVersion;
	std::string cueName;
	bool loop = false;
	float duration = 0.0f;
	std::vector<VfxCueTrackDesc> tracks;
};

inline VfxCueTrackDesc CreateDefaultVfxCueTrack(VfxCueTrackType type)
{
	VfxCueTrackDesc track{};
	track.type = type;

	switch (type)
	{
	case VfxCueTrackType::Fluid2D:
	case VfxCueTrackType::VolumetricFluid:
		track.payload = VfxFluidTrackPayload{};
		break;
	case VfxCueTrackType::Light:
		track.payload = VfxLightTrackPayload{};
		break;
	case VfxCueTrackType::PostEffect:
		track.payload = VfxPostEffectTrackPayload{};
		break;
	case VfxCueTrackType::CameraShake:
		track.payload = VfxCameraShakeTrackPayload{};
		break;
	case VfxCueTrackType::Particle:
	default:
		track.payload = VfxParticleTrackPayload{};
		break;
	}

	return track;
}

} // namespace Ken4lowEngine
