#include "GpuParticleEmitter.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
namespace
{
	constexpr uint32_t kDescOverrideFlag = 1u << 0;
	constexpr uint32_t kSizeCurveFlag = 1u << 2;
	constexpr uint32_t kColorGradientFlag = 1u << 3;

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

GpuParticleEmitter::GpuParticleEmitter(const std::string& name, const EmitterInfo& info)
	: name_(name), info_(info)
{
}

uint32_t GpuParticleEmitter::RequestEmit(uint32_t count)
{
	if (count == 0 || info_.maxParticles == 0) return 0;

	// Runtime EmitterがmaxParticlesを超えて無限に増えないよう、CPU推定生存数と発生待ち数で抑制する。
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
		return (std::max)(info_.lifeTime + std::abs(info_.lifeTimeRandom), 0.01f) + 0.10f;
	}

	return EstimateGpuParticleLifeTimeSec(static_cast<GpuParticleType>(GetEffectiveType())) * std::max(info_.lifeScale, 0.01f) + 0.10f;
}

void GpuParticleEmitter::RegisterActiveBatch(uint32_t count)
{
	if (count == 0) return;
	activeBatches_.push_back({ EstimateParticleLifeTimeSec(), count });
	estimatedActiveParticleCount_ += count;
}

bool GpuParticleEmitter::BuildCB(GpuEmitterCBData& out, float deltaTime)
{
	if (info_.loopFrequency > 0.0f && info_.loopCount > 0)
	{
		float scheduleDelta = (std::max)(deltaTime, 0.0f);
		if (!info_.loopForever)
		{
			const float remaining = (std::max)(info_.emissionDuration - emissionElapsed_, 0.0f);
			scheduleDelta = (std::min)(scheduleDelta, remaining);
			emissionElapsed_ += scheduleDelta;
		}

		// 有限durationでは最後の端数フレームを切り詰め、指定時間を超えた定期発生を行わない。
		loopTimer_ += scheduleDelta;
		while (loopTimer_ >= info_.loopFrequency && scheduleDelta > 0.0f)
		{
			RequestEmit(info_.loopCount);
			loopTimer_ -= info_.loopFrequency;
		}
	}

	out.translate = position_;
	out.radius = info_.radius;
	out.frequency = info_.loopFrequency;
	out.frequencyTime = loopTimer_;
	out.billboardMode = GetPackedBillboardMode();
	out.lifeScale = std::max(info_.lifeScale, 0.01f);
	out.speedScale = std::max(info_.speedScale, 0.0f);
	out.overrideFlags = 0u;
	if (info_.useDescSpawnOverride) out.overrideFlags |= kDescOverrideFlag;
	if (info_.useSizeCurve) out.overrideFlags |= kSizeCurveFlag;
	if (info_.useColorGradient) out.overrideFlags |= kColorGradientFlag;
	out.maxParticles = info_.maxParticles;

	const uint32_t packedDrawType = GetDrawType();
	out.renderGroup = BuildGpuParticleRenderGroup(
		info_.textureFilePath,
		UnpackGpuParticleMaterialDrawType(packedDrawType),
		UnpackGpuParticleBlendMode(packedDrawType));
	// Desc Overrideはlegacy Type Presetを使わないためtype channelをMaterial renderGroupとして再利用し、Spawn shader変更なしで正しいTexture選別を保証する。
	out.type = info_.useDescSpawnOverride ? out.renderGroup : GetEffectiveType();

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

	out.sizeCurveLut = info_.sizeCurveLut;
	out.colorGradientLut0 = info_.colorGradientLut[0];
	out.colorGradientLut1 = info_.colorGradientLut[1];
	out.colorGradientLut2 = info_.colorGradientLut[2];
	out.colorGradientLut3 = info_.colorGradientLut[3];
	out.noiseStrength = info_.noiseStrength;
	out.noiseFrequency = (std::max)(info_.noiseFrequency, 0.0f);
	out.vortexAxis = info_.vortexAxis;
	out.vortexStrength = info_.vortexStrength;
	out.attractorPosition = info_.attractorPosition;
	out.attractorStrength = info_.attractorStrength;
	out.attractorRadius = (std::max)(info_.attractorRadius, 0.0f);
	out.startRotation3D = info_.startRotation3D;
	out.rotationRandom3D = info_.rotationRandom3D;
	out.angularVelocity = info_.angularVelocity;
	out.angularVelocityRandom = info_.angularVelocityRandom;

	if (pendingBurstCount_ == 0)
	{
		out.emit = 0;
		out.count = 0;
		return false;
	}

	out.emit = 1;
	out.count = pendingBurstCount_;
	out.frequencyTime = 0.0f;
	RegisterActiveBatch(pendingBurstCount_);
	pendingBurstCount_ = 0;
	return true;
}


} // namespace Ken4lowEngine
