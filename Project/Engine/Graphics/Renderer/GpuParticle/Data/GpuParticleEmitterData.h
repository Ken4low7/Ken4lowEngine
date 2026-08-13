#pragma once
#include <cstdint>
#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

#include "GpuParticleType.h"
#include "BillboardMode.h"
#include "BlendModeType.h"

namespace Ken4lowEngine
{

// HLSL EmitterCBDataと同一順序で保持し、Runtime Authoring値をEmit Computeへ渡す。
struct GpuEmitterCBData
{
	Vector3 translate;
	float radius;
	uint32_t count;
	float frequency;
	float frequencyTime;
	uint32_t emit;
	uint32_t type;
	uint32_t billboardMode;
	float lifeScale = 1.0f;
	float speedScale = 1.0f;
	uint32_t overrideFlags = 0;
	uint32_t maxParticles = UINT32_MAX;
	float overridePadding[2]{};
	Vector3 positionRandom{};
	float lifeTime = 1.0f;
	Vector3 velocity{};
	float lifeTimeRandom = 0.0f;
	Vector3 velocityRandom{};
	float sizeRandom = 0.0f;
	Vector2 startSize{ 1.0f, 1.0f };
	Vector2 endSize{ 1.0f, 1.0f };
	Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
	Vector3 gravity{};
	float damping = 0.0f;
	float speed = 0.0f;
	float speedRandom = 0.0f;
	float startRotation = 0.0f;
	float rotationSpeed = 0.0f;
	float rotationRandom = 0.0f;
	float spawnRadius = 0.0f;
	uint32_t spawnShape = 0;
	uint32_t alphaFade = 1;
	Vector3 spawnBoxSize{};
	float spawnBoxPadding = 0.0f;
	Vector4 colorRandom{};
	Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
	float startScalePadding = 0.0f;
	Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
	float endScalePadding = 0.0f;
	uint32_t useSpriteSheet = 0;
	uint32_t spriteSheetRows = 1;
	uint32_t spriteSheetColumns = 1;
	float spriteSheetFrameRate = 0.0f;

	Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 colorGradientLut0{ 1.0f, 1.0f, 1.0f, 1.0f };
	Vector4 colorGradientLut1{ 1.0f, 1.0f, 1.0f, 0.66f };
	Vector4 colorGradientLut2{ 1.0f, 1.0f, 1.0f, 0.33f };
	Vector4 colorGradientLut3{ 1.0f, 1.0f, 1.0f, 0.0f };
	float noiseStrength = 0.0f;
	float noiseFrequency = 1.0f;
	float vortexStrength = 0.0f;
	float attractorStrength = 0.0f;
	Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f };
	float attractorRadius = 0.0f;
	Vector3 attractorPosition{};
	float forcePadding = 0.0f;
	Vector3 startRotation3D{};
	float rotation3DPadding = 0.0f;
	Vector3 rotationRandom3D{};
	float rotationRandom3DPadding = 0.0f;
	Vector3 angularVelocity{};
	float angularVelocityPadding = 0.0f;
	Vector3 angularVelocityRandom{};
	float angularVelocityRandomPadding = 0.0f;
};

// drawTypeの上位4bitへBlendModeを埋め込み、既存Managerの描画重複排除キーもそのまま利用する。
inline constexpr uint32_t kGpuParticleBlendTagShift = 28u;
inline constexpr uint32_t kGpuParticleBlendTagMask = 0xF0000000u;
inline constexpr uint32_t kGpuParticleMaterialDrawTypeMask = 0x0FFFFFFFu;

inline constexpr uint32_t PackGpuParticleDrawType(uint32_t materialDrawType, BlendMode blendMode)
{
	const uint32_t blendTag = static_cast<uint32_t>(blendMode) + 1u;
	return (materialDrawType & kGpuParticleMaterialDrawTypeMask) |
		((blendTag << kGpuParticleBlendTagShift) & kGpuParticleBlendTagMask);
}

inline constexpr uint32_t UnpackGpuParticleMaterialDrawType(uint32_t packedDrawType)
{
	return packedDrawType & kGpuParticleMaterialDrawTypeMask;
}

inline constexpr BlendMode UnpackGpuParticleBlendMode(uint32_t packedDrawType)
{
	const uint32_t blendTag = (packedDrawType & kGpuParticleBlendTagMask) >> kGpuParticleBlendTagShift;
	if (blendTag == 0u)
	{
		return BlendMode::kBlendModeAdd;
	}

	const uint32_t decoded = blendTag - 1u;
	const uint32_t maxMode = static_cast<uint32_t>(BlendMode::kcountOfBlendMode) - 1u;
	return decoded <= maxMode ? static_cast<BlendMode>(decoded) : BlendMode::kBlendModeAdd;
}

static_assert(sizeof(GpuEmitterCBData) == 480); // HLSL cbufferと16byteパッキングを一致させる。
} // namespace Ken4lowEngine
