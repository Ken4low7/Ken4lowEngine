#define NOMINMAX
#include "BossAttackEffects.h"
#include "GpuParticleType.h"
#include "GpuParticleManager.h"
#include "GpuParticleEmitter.h"

#include <algorithm>
#include <string>

namespace
{
	void EmitGpuParticle(
		const char* emitterName,
		Ken4lowEngine::GpuParticleKind kind,
		Ken4lowEngine::GpuParticleType particleType,
		const Ken4lowEngine::Vector3& position,
		uint32_t spawnCount,
		float spawnRadius,
		float lifetimeScale,
		float initialSpeedScale)
	{
		if (spawnCount == 0 || emitterName == nullptr) return;

		auto* particleManager = Ken4lowEngine::GpuParticleManager::GetInstance();
		if (!particleManager) return;

		Ken4lowEngine::GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = "Effects/white.dds";
		info.radius = std::max(0.0f, spawnRadius);
		info.kind = kind;
		info.spriteType = particleType;
		info.billboardFlags = Ken4lowEngine::BillboardMode::Camera;
		info.lifeScale = std::max(0.01f, lifetimeScale);
		info.speedScale = std::max(0.0f, initialSpeedScale);

		if (auto* emitter = particleManager->GetEmitter(emitterName))
		{
			emitter->GetInfoMutable() = info;
			emitter->SetPosition(position);
			emitter->RequestEmit(spawnCount);
			return;
		}

		if (auto* created = particleManager->CreateEmitter(emitterName, info))
		{
			created->SetPosition(position);
			created->RequestEmit(spawnCount);
		}
	}
}

void BossAttackEffects::EmitGuardianHitEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	// SpriteとMeshを同時に出し、煙だけでなく石片の重さも足す。
	EmitGpuParticle(emitterName, Ken4lowEngine::GpuParticleKind::Sprite, particleType, position, spawnCount, spawnRadius, lifetimeScale, initialSpeedScale);

	const std::string meshEmitterName = std::string(emitterName) + "MeshDebris";
	EmitGuardianMeshDebrisEffect(meshEmitterName.c_str(), Ken4lowEngine::GpuParticleType::Debris, position, std::max<uint32_t>(6, spawnCount / 3), spawnRadius * 0.85f, lifetimeScale * 1.15f, initialSpeedScale * 1.35f);
}

void BossAttackEffects::EmitGuardianMeshDebrisEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	// Meshパーティクルは瓦礫・石片用として使う。
	EmitGpuParticle(emitterName, Ken4lowEngine::GpuParticleKind::Mesh, particleType, position, spawnCount, spawnRadius, lifetimeScale, initialSpeedScale);
}

void BossAttackEffects::EmitGuardianAttackPresenceEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	// 予兆や移動中にもSpriteとMeshを混ぜて存在感を出す。
	EmitGpuParticle(emitterName, Ken4lowEngine::GpuParticleKind::Sprite, particleType, position, spawnCount, spawnRadius, lifetimeScale, initialSpeedScale);

	const std::string meshEmitterName = std::string(emitterName) + "MeshDebris";
	EmitGuardianMeshDebrisEffect(meshEmitterName.c_str(), Ken4lowEngine::GpuParticleType::Debris, position, std::max<uint32_t>(2, spawnCount / 2), spawnRadius * 0.65f, lifetimeScale, initialSpeedScale * 1.15f);
}

void BossAttackEffects::EmitGuardianTelegraphEffect(const char* emitterName, Ken4lowEngine::GpuParticleType particleType, const Ken4lowEngine::Vector3& position, uint32_t spawnCount, float spawnRadius, float lifetimeScale, float initialSpeedScale)
{
	EmitGpuParticle(emitterName, Ken4lowEngine::GpuParticleKind::Sprite, particleType, position, spawnCount, spawnRadius, lifetimeScale, initialSpeedScale); // Windup中はSpriteだけを使い、連続予兆でMesh破片が増えすぎないようにする。
}