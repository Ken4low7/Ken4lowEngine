#include "GpuParticleEmitter.h"

#include <algorithm>

namespace Ken4lowEngine
{
namespace
{
	float EstimateGpuParticleLifeTimeSec(GpuParticleType type)
	{
		switch (type)
		{
		case GpuParticleType::Blood: return 0.80f;
		case GpuParticleType::Dust: return 1.20f;
		case GpuParticleType::Debris: return 1.00f;
		case GpuParticleType::Smoke: return 3.00f;
		case GpuParticleType::Ambient: return 7.00f;
		case GpuParticleType::Spark: return 0.22f;
		case GpuParticleType::Shockwave: return 0.50f;
		case GpuParticleType::Heal: return 1.40f;
		case GpuParticleType::Trail: return 0.22f;
		case GpuParticleType::DeathBurstCore: return 0.28f;
		case GpuParticleType::PlayerDamageBlood: return 0.55f;
		case GpuParticleType::MuzzleFlash: return 0.085f;
		case GpuParticleType::BulletTracer: return 0.30f;
		case GpuParticleType::Default:
		default:
			return 1.00f;
		}
	}
}

/// -------------------------------------------------------------
///					　　　コンストラクタ
/// -------------------------------------------------------------
GpuParticleEmitter::GpuParticleEmitter(const std::string& name, const EmitterInfo& info)
	: name_(name), info_(info)
{
}

/// -------------------------------------------------------------
///					　　　射出要求
/// -------------------------------------------------------------
uint32_t GpuParticleEmitter::RequestEmit(uint32_t count)
{
	if (count == 0 || info_.maxParticles == 0) return 0;

	// Preview EmitterがmaxParticlesを超えて無限に増えないよう、CPU推定生存数と発生待ち数で抑制する。
	const uint64_t reserved = static_cast<uint64_t>(estimatedActiveParticleCount_) + pendingBurstCount_;
	const uint64_t available = reserved < info_.maxParticles ? info_.maxParticles - reserved : 0;
	const uint32_t accepted = static_cast<uint32_t>((std::min)(static_cast<uint64_t>(count), available));
	pendingBurstCount_ += accepted;
	return accepted;
}

void GpuParticleEmitter::UpdateActivity(float deltaTime)
{
	for (auto& batch : activeBatches_)
	{
		batch.remainingSec -= deltaTime;
	}

	while (!activeBatches_.empty() && activeBatches_.front().remainingSec <= 0.0f)
	{
		estimatedActiveParticleCount_ -= std::min(estimatedActiveParticleCount_, activeBatches_.front().count);
		activeBatches_.pop_front();
	}
}

float GpuParticleEmitter::EstimateParticleLifeTimeSec() const
{
	if (info_.useDescSpawnOverride)
	{
		// Preview粒子はDescのlifeTimeをGPUへ直接渡すため、CPU側の生存数推定も同じ寿命へ合わせる。
		return (std::max)(info_.lifeTime + std::abs(info_.lifeTimeRandom), 0.01f) + 0.10f;
	}

	// HLSL の lifeMax と合わせ、寿命後は Draw 対象から外して常時フル描画を避ける。
	return EstimateGpuParticleLifeTimeSec(static_cast<GpuParticleType>(GetEffectiveType())) * std::max(info_.lifeScale, 0.01f) + 0.10f;
}

void GpuParticleEmitter::RegisterActiveBatch(uint32_t count)
{
	if (count == 0)
	{
		return;
	}

	activeBatches_.push_back({ EstimateParticleLifeTimeSec(), count });
	estimatedActiveParticleCount_ += count;
}

/// -------------------------------------------------------------
///				　　定期発射の更新
/// -------------------------------------------------------------
bool GpuParticleEmitter::BuildCB(GpuEmitterCBData& out, float deltaTime)
{
	// ------------------------------
	// ループ発生（定期発生）
	// ------------------------------
	if (info_.loopFrequency > 0.0f && info_.loopCount > 0)
	{
		loopTimer_ += deltaTime;

		// 周期を超えた分だけ発生（取りこぼし防止でwhile）
		while (loopTimer_ >= info_.loopFrequency)
		{
			RequestEmit(info_.loopCount);
			loopTimer_ -= info_.loopFrequency;
		}
	}

	// ------------------------------
	// 共通でCBに書く値（Emitしない場合も）
	// ------------------------------
	out.translate = position_;
	out.radius = info_.radius;
	out.frequency = info_.loopFrequency;
	out.frequencyTime = loopTimer_; // デバッグ用に残す（不要なら0でもOK）

	// kindに応じた type / packed billboardMode
	out.type = GetEffectiveType();
	out.billboardMode = GetPackedBillboardMode();
	out.lifeScale = std::max(info_.lifeScale, 0.01f);
	out.speedScale = std::max(info_.speedScale, 0.0f);
	out.overrideFlags = info_.useDescSpawnOverride ? 1u : 0u;
	out.maxParticles = info_.maxParticles;
	out.positionRandom = info_.positionRandom;
	out.lifeTime = (std::max)(info_.lifeTime, 0.01f);
	out.velocity = info_.velocity;
	out.lifeTimeRandom = (std::max)(info_.lifeTimeRandom, 0.0f);
	out.velocityRandom = info_.velocityRandom;
	out.sizeRandom = (std::max)(info_.sizeRandom, 0.0f);
	out.startSize = info_.startSize;
	out.endSize = info_.endSize;
	out.startColor = info_.startColor;
	out.endColor = info_.endColor;
	out.gravity = info_.gravity;
	out.damping = (std::max)(info_.damping, 0.0f);
	out.speed = (std::max)(info_.speed, 0.0f);
	out.speedRandom = (std::max)(info_.speedRandom, 0.0f);
	out.startRotation = info_.startRotation;
	out.rotationSpeed = info_.rotationSpeed;
	out.rotationRandom = (std::max)(info_.rotationRandom, 0.0f);
	out.spawnRadius = (std::max)(info_.spawnRadius, 0.0f);
	out.spawnShape = info_.spawnShape;
	out.alphaFade = info_.alphaFade ? 1u : 0u;
	out.spawnBoxSize = info_.spawnBoxSize;
	out.colorRandom = info_.colorRandom;
	out.startScale3D = info_.startScale3D;
	out.endScale3D = info_.endScale3D;
	out.useSpriteSheet = info_.useSpriteSheet ? 1u : 0u;
	out.spriteSheetRows = (std::max)(info_.spriteSheetRows, 1u);
	out.spriteSheetColumns = (std::max)(info_.spriteSheetColumns, 1u);
	out.spriteSheetFrameRate = (std::max)(info_.spriteSheetFrameRate, 0.0f);

	// ------------------------------
	// Emitしない
	// ------------------------------
	if (pendingBurstCount_ == 0)
	{
		out.emit = 0;
		out.count = 0;
		return false;
	}

	// ------------------------------
	// Emitする
	// ------------------------------
	out.emit = 1;
	out.count = pendingBurstCount_;
	out.frequencyTime = 0.0f; // 使わないなら0固定でOK

	// 生存数をCPU側でも概算追跡し、寿命終了後の不要な全エミッター描画を止める。
	RegisterActiveBatch(pendingBurstCount_);

	// 消費（このフレーム分を確定）
	pendingBurstCount_ = 0;

	return true;
}


} // namespace Ken4lowEngine
