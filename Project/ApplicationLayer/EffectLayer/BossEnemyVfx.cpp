#include "BossEnemyVfx.h"
#include <algorithm>
#include <cmath>

namespace K4E = ::Ken4lowEngine;

void BossEnemyVfx::UpdateAppearDust(const K4E::Vector3& position, uint32_t count)
{
	if (!appearDustEmitter_)
	{
		appearDustEmitter_ = FindEmitter("BossAppear");
		if (!appearDustEmitter_) return;
	}

	SetPositionAndEmit(appearDustEmitter_, position, count);
	// 登場土煙と同時にMesh破片も発生させ、地面から出てきた重量感を足す。
	UpdateAppearMeshDebris(position, std::max<uint32_t>(3, count / 3));
}

void BossEnemyVfx::UpdateAppearMeshDebris(const K4E::Vector3& position, uint32_t count)
{
	if (!appearMeshDebrisEmitter_)
	{
		appearMeshDebrisEmitter_ = FindEmitter("BossAppearMeshDebris");
		if (!appearMeshDebrisEmitter_) return;
	}

	SetPositionAndEmit(appearMeshDebrisEmitter_, position, count);
}

void BossEnemyVfx::UpdateAura(const K4E::Vector3& position, uint32_t count)
{
	if (!auraEmitter_)
	{
		auraEmitter_ = FindEmitter("BossAura");
		if (!auraEmitter_) return;
	}
	SetPositionAndEmit(auraEmitter_, position, count);
}

void BossEnemyVfx::UpdateRushTrail(float deltaTime, const K4E::Vector3& currentPosition, const K4E::Vector3& oldPosition, bool isRush)
{
	if (!isRush) return;

	if (!rushTrailEmitter_)
	{
		rushTrailEmitter_ = FindEmitter("BossRushTrail");
		if (!rushTrailEmitter_) return;
	}

	rushTrailEmitter_->SetPosition(currentPosition);
	K4E::Vector3 d = currentPosition - oldPosition;
	float distXZ = std::sqrt(d.x * d.x + d.z * d.z);
	float dt = (deltaTime > 0.00001f) ? deltaTime : 0.00001f;
	float speed = distXZ / dt;
	uint32_t emitCount = static_cast<uint32_t>(std::clamp(speed, 15.0f, 50.0f));
	rushTrailEmitter_->RequestEmit(emitCount);
}

void BossEnemyVfx::UpdateRushHit(const K4E::Vector3& hitPosition, const K4E::Vector3& bossCenter)
{
	if (!rushHitEmitter_)
	{
		rushHitEmitter_ = FindEmitter("BossRushHit");
		if (!rushHitEmitter_) return;
	}
	if (rushHitCooldown_ > 0.0f) return;

	K4E::Vector3 pos = hitPosition;
	pos.y = bossCenter.y - 2.0f + 0.05f;
	rushHitEmitter_->SetPosition(pos);
	rushHitEmitter_->RequestEmit(64);
	rushHitCooldown_ = 0.20f;
}

void BossEnemyVfx::UpdateSpinAttack(const K4E::Vector3& currentPosition, bool isSpin)
{
	if (!isSpin) return;

	if (!spinAttackEmitter_)
	{
		spinAttackEmitter_ = FindEmitter("BossSpinAttack");
		if (!spinAttackEmitter_) return;
	}

	SetPositionAndEmit(spinAttackEmitter_, currentPosition, 10);
}

static K4E::Vector3 MakeShockwavePosition(const K4E::Vector3& center)
{
	K4E::Vector3 ringPos = center;
	ringPos.y -= 1.8f;
	return ringPos;
}

void BossEnemyVfx::UpdateDeathEffect(const K4E::Vector3& center, float deathTimer, bool& startBurstDone)
{
	K4E::Vector3 core = center;
	core.y += 1.0f;

	if (!startBurstDone)
	{
		// 死亡開始時は爆発・衝撃波・Mesh破片を同時に大量発生させ、ボス撃破の達成感を強める。
		if (deathExplosionEmitter_) SetPositionAndEmit(deathExplosionEmitter_, core, 48);
		if (deathShockwaveEmitter_) SetPositionAndEmit(deathShockwaveEmitter_, MakeShockwavePosition(center), 18);
		if (debrisDustEmitter_) SetPositionAndEmit(debrisDustEmitter_, center, 160);

		K4E::Vector3 leftBurst = core;
		leftBurst.x -= 1.8f;
		if (deathExplosionEmitter_) SetPositionAndEmit(deathExplosionEmitter_, leftBurst, 18);
		K4E::Vector3 rightBurst = core;
		rightBurst.x += 1.8f;
		if (deathExplosionEmitter_) SetPositionAndEmit(deathExplosionEmitter_, rightBurst, 18);
		K4E::Vector3 backBurst = core;
		backBurst.z -= 1.8f;
		if (deathExplosionEmitter_) SetPositionAndEmit(deathExplosionEmitter_, backBurst, 18);
		K4E::Vector3 frontBurst = core;
		frontBurst.z += 1.8f;
		if (deathExplosionEmitter_) SetPositionAndEmit(deathExplosionEmitter_, frontBurst, 18);

		startBurstDone = true;
	}

	if (deathTimer < 1.8f)
	{
		// 死亡中は中心から煙・魂・破片を継続的に出し、消滅中も画面に迫力を残す。
		if (deathSoulEmitter_) SetPositionAndEmit(deathSoulEmitter_, core, 4);
		if (debrisDustEmitter_) SetPositionAndEmit(debrisDustEmitter_, center, 8);
		if (deathExplosionEmitter_ && deathTimer > 0.35f && deathTimer < 0.55f) SetPositionAndEmit(deathExplosionEmitter_, core, 14);
		if (deathShockwaveEmitter_ && deathTimer > 0.70f && deathTimer < 0.90f) SetPositionAndEmit(deathShockwaveEmitter_, MakeShockwavePosition(center), 10);
	}
}

void BossEnemyVfx::RegisterEmitters()
{
	Reset();
	const char* kDefaultTex = "Effects/white.dds";

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Dust;
		info.radius = 3.0f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		appearDustEmitter_ = GetOrCreateEmitter("BossAppear", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Mesh;
		info.spriteType = K4E::GpuParticleType::Debris;
		info.radius = 2.2f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		info.lifeScale = 1.35f;
		info.speedScale = 1.6f;
		appearMeshDebrisEmitter_ = GetOrCreateEmitter("BossAppearMeshDebris", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Debris;
		info.radius = 0.5f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		auraEmitter_ = GetOrCreateEmitter("BossAura", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Trail;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		rushTrailEmitter_ = GetOrCreateEmitter("BossRushTrail", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Shockwave;
		info.radius = 0.5f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		rushHitEmitter_ = GetOrCreateEmitter("BossRushHit", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Default;
		info.radius = 0.1f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		spinAttackEmitter_ = GetOrCreateEmitter("BossSpinAttack", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Default;
		info.radius = 1.4f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		info.lifeScale = 1.2f;
		info.speedScale = 2.2f;
		deathExplosionEmitter_ = GetOrCreateEmitter("BossDeathExplosion", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Shockwave;
		info.radius = 0.35f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		info.lifeScale = 1.1f;
		info.speedScale = 1.8f;
		deathShockwaveEmitter_ = GetOrCreateEmitter("BossDeathShockwave", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Sprite;
		info.spriteType = K4E::GpuParticleType::Default;
		info.radius = 0.75f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		info.lifeScale = 1.6f;
		info.speedScale = 0.7f;
		deathSoulEmitter_ = GetOrCreateEmitter("BossDeathSoul", info);
	}

	{
		K4E::GpuParticleEmitter::EmitterInfo info{};
		info.kind = K4E::GpuParticleKind::Mesh;
		info.spriteType = K4E::GpuParticleType::Debris;
		info.radius = 3.2f;
		info.billboardFlags = K4E::BillboardMode::Camera;
		info.textureFilePath = kDefaultTex;
		info.lifeScale = 1.8f;
		info.speedScale = 2.4f;
		debrisDustEmitter_ = GetOrCreateEmitter("BossDebrisDust", info);
	}
}
