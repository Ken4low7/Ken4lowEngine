#pragma once
#include <array>
#include <deque>
#include <limits>
#include <string>
#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

#include "GpuParticleType.h"
#include "GpuParticleEmitterData.h"
#include "BillboardMode.h"

namespace Ken4lowEngine
{

enum class GpuParticleForwardDrawPass : uint8_t
{
	All = 0,
	Transparent,
	Additive,
};

class GpuParticleEmitter
{
public:
	struct EmitterInfo
	{
		std::string textureFilePath;
		float radius = 0.0f;
		uint32_t loopCount = 0;
		float loopFrequency = 0.0f;
		bool loopForever = true;
		float emissionDuration = 0.0f;

		uint32_t drawType = 0;
		GpuParticleKind kind = GpuParticleKind::Sprite;
		GpuParticleType spriteType = GpuParticleType::Default;
		GpuRibbonType ribbonType = GpuRibbonType::Trail;
		BillboardMode billboardFlags = BillboardMode::Camera;
		float lifeScale = 1.0f;
		float speedScale = 1.0f;

		bool useDescSpawnOverride = false;
		uint32_t maxParticles = (std::numeric_limits<uint32_t>::max)();
		Vector3 positionRandom{};
		Vector3 velocity{};
		Vector3 velocityRandom{};
		Vector2 startSize{ 1.0f, 1.0f };
		Vector2 endSize{ 1.0f, 1.0f };
		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		float lifeTime = 1.0f;
		float lifeTimeRandom = 0.0f;
		Vector3 gravity{};
		float damping = 0.0f;
		float speed = 0.0f;
		float speedRandom = 0.0f;
		float sizeRandom = 0.0f;
		float startRotation = 0.0f;
		float rotationSpeed = 0.0f;
		float rotationRandom = 0.0f;
		uint32_t spawnShape = 0;
		float spawnRadius = 0.0f;
		Vector3 spawnBoxSize{};
		Vector4 colorRandom{};
		bool alphaFade = true;
		Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
		Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
		bool useSpriteSheet = false;
		uint32_t spriteSheetRows = 1;
		uint32_t spriteSheetColumns = 1;
		float spriteSheetFrameRate = 0.0f;

		bool useSizeCurve = false;
		Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f };
		bool useColorGradient = false;
		std::array<Vector4, 4> colorGradientLut{
			Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.66f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.33f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.0f }
		};

		float noiseStrength = 0.0f;
		float noiseFrequency = 1.0f;
		Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f };
		float vortexStrength = 0.0f;
		Vector3 attractorPosition{};
		float attractorStrength = 0.0f;
		float attractorRadius = 0.0f;
		Vector3 startRotation3D{};
		Vector3 rotationRandom3D{};
		Vector3 angularVelocity{};
		Vector3 angularVelocityRandom{};

		uint32_t collisionShape = 0u;
		uint32_t collisionResponse = 0u;
		Vector3 collisionPlaneNormal{ 0.0f, 1.0f, 0.0f };
		float collisionPlaneDistance = 0.0f;
		Vector3 collisionSphereCenter{};
		float collisionSphereRadius = 1.0f;
		float collisionParticleRadius = 0.02f;
		float collisionRestitution = 0.5f;
		float collisionFriction = 0.1f;
		uint32_t eventMask = 0u;
		uint32_t subEmitterEventMask = 0u;
		uint32_t subEmitterCount = 0u;
		float subEmitterLifeTime = 0.2f;
		float subEmitterSpeed = 1.5f;
		float subEmitterSpread = 1.0f;
		float subEmitterInheritVelocity = 0.2f;
		Vector2 subEmitterStartSize{ 0.04f, 0.04f };
		Vector2 subEmitterEndSize{ 0.12f, 0.12f };
		Vector4 subEmitterStartColor{ 1.0f, 0.8f, 0.2f, 1.0f };
		Vector4 subEmitterEndColor{ 1.0f, 0.1f, 0.0f, 0.0f };
		bool subEmitterAlphaFade = true;
	};

public:
	GpuParticleEmitter(const std::string& name, const EmitterInfo& info);
	uint32_t RequestEmit(uint32_t count);
	void UpdateActivity(float deltaTime);
	bool BuildCB(GpuEmitterCBData& out, float deltaTime);

	void ResetEmissionSchedule()
	{
		loopTimer_ = 0.0f;
		emissionElapsed_ = 0.0f;
	}

	static void SetForwardDrawPass(GpuParticleForwardDrawPass pass) { forwardDrawPass_ = pass; }
	static GpuParticleForwardDrawPass GetForwardDrawPass() { return forwardDrawPass_; }
	void SetPosition(const Vector3& position) { position_ = position; }
	const std::string& GetName() const { return name_; }
	const EmitterInfo& GetInfo() const { return info_; }

	uint32_t GetDrawType() const
	{
		const uint32_t effectiveType = GetEffectiveType();
		return (info_.drawType != 0) ? info_.drawType : effectiveType;
	}

	EmitterInfo& GetInfoMutable() { return info_; }
	const Vector3 GetPosition() const { return position_; }
	uint32_t GetEstimatedActiveParticleCount() const { return estimatedActiveParticleCount_; }

	bool HasEmissionSchedule() const
	{
		if (info_.loopCount == 0 || info_.loopFrequency <= 0.0f) return false;
		return info_.loopForever || emissionElapsed_ < info_.emissionDuration;
	}

	bool HasRuntimeActivity() const
	{
		return estimatedActiveParticleCount_ > 0 || pendingBurstCount_ > 0 || HasEmissionSchedule();
	}

	bool MatchesForwardDrawPass() const
	{
		const BlendMode blendMode = UnpackGpuParticleBlendMode(GetDrawType());
		switch (forwardDrawPass_)
		{
		case GpuParticleForwardDrawPass::Transparent: return blendMode != BlendMode::kBlendModeAdd;
		case GpuParticleForwardDrawPass::Additive: return blendMode == BlendMode::kBlendModeAdd;
		case GpuParticleForwardDrawPass::All:
		default: return true;
		}
	}

	bool HasActiveParticles() const
	{
		if (!MatchesForwardDrawPass()) return false;
		return HasRuntimeActivity();
	}

private:
	static constexpr uint32_t ToU32(BillboardMode m) { return static_cast<uint32_t>(m); }

	uint32_t GetEffectiveType() const
	{
		if (info_.kind == GpuParticleKind::Sprite) return static_cast<uint32_t>(info_.spriteType);
		if (info_.kind == GpuParticleKind::Ribbon) return static_cast<uint32_t>(ToGpuParticleType(info_.ribbonType));
		return static_cast<uint32_t>(info_.spriteType);
	}

	uint32_t GetPackedBillboardMode() const
	{
		return PackBillboardMode(info_.kind, ToU32(info_.billboardFlags));
	}

private:
	std::string name_;
	EmitterInfo info_;
	Vector3 position_{ 0.0f, 0.0f, 0.0f };
	float loopTimer_ = 0.0f;
	float emissionElapsed_ = 0.0f;
	uint32_t pendingBurstCount_ = 0;
	bool initialLoopRequestPending_ = true;

	struct ActiveBatch
	{
		float remainingSec = 0.0f;
		uint32_t count = 0;
	};

	float EstimateParticleLifeTimeSec() const;
	void RegisterActiveBatch(uint32_t count);

	std::deque<ActiveBatch> activeBatches_;
	uint32_t estimatedActiveParticleCount_ = 0;
	inline static thread_local GpuParticleForwardDrawPass forwardDrawPass_ = GpuParticleForwardDrawPass::All;
};

} // namespace Ken4lowEngine
