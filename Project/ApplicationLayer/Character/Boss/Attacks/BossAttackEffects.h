#pragma once

#include <Vector3.h>
#include <cstdint>

namespace Ken4lowEngine
{
	enum class GpuParticleType : uint32_t;
}

namespace BossAttackEffects
{
	void EmitGuardianHitEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);
	void EmitGuardianMeshDebrisEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);
	void EmitGuardianAttackPresenceEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);
	void EmitGuardianTelegraphEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale);
}