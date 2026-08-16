#pragma once

#include "../Data/VfxCueTypes.h"
#include "Vector3.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{

struct VfxCueHandle
{
	uint64_t value = 0;

	[[nodiscard]] bool IsValid() const { return value != 0; }
	friend bool operator==(const VfxCueHandle&, const VfxCueHandle&) = default;
};

struct VfxTrackRuntimeToken
{
	uint64_t value = 0;
	[[nodiscard]] bool IsValid() const { return value != 0; }
};

struct VfxResolvedTrackParameters
{
	float intensityScale = 1.0f;
	float radiusScale = 1.0f;
	std::unordered_map<std::string, float> particleFloatOverrides;
};

struct VfxRuntimeBudget
{
	uint32_t maxActiveInstances = 128;
	uint32_t maxTrackStartsPerFrame = 64;
	uint32_t maxActiveTracks = 512;
	uint32_t maxTransientLights = 32;
	uint32_t maxFluidTracks = 64;
	uint32_t maxCameraShakes = 16;
	uint32_t maxVfxGraphStartCostPerFrame = 64;
	uint32_t maxActiveVfxGraphLoopCost = 128;
};

struct VfxRuntimeStats
{
	uint64_t totalPlayRequests = 0;
	uint64_t totalStopRequests = 0;
	uint64_t totalTrackStarts = 0;
	uint64_t totalTrackStops = 0;
	uint64_t completedInstances = 0;
	uint64_t adapterFailures = 0;
	uint64_t budgetRejectedInstances = 0;
	uint64_t budgetDelayedTrackStarts = 0;
	uint64_t hotReloadCount = 0;
	uint64_t stressPlayCount = 0;
	uint32_t registeredCueCount = 0;
	uint32_t activeInstanceCount = 0;
	uint32_t activeTrackCount = 0;
	uint32_t activeParticleTrackCount = 0;
	uint32_t activeFluid2DTrackCount = 0;
	uint32_t activeVolumetricTrackCount = 0;
	uint32_t activeLightTrackCount = 0;
	uint32_t activePostEffectTrackCount = 0;
	uint32_t activeCameraShakeTrackCount = 0;
	uint32_t peakActiveInstanceCount = 0;
	uint32_t peakActiveTrackCount = 0;
	uint32_t trackStartsThisFrame = 0;
	bool lastOperationSucceeded = true;
	std::string lastStatus;
};

struct VfxTrackStartContext
{
	VfxCueHandle cueHandle{};
	uint64_t runtimeTrackId = 0;
	VfxCueTrackType type = VfxCueTrackType::Particle;
	Vector3 worldPosition{};
	Vector3 localOffset{};
	VfxCueTrackPayload payload = VfxParticleTrackPayload{};
	VfxResolvedTrackParameters parameters{};
};

} // namespace Ken4lowEngine
