#include "GpuParticleData.hlsli"

struct ParticleTypeParam
{
    float baseAlpha;
    float alphaPow;
    float accelY;
    float drag;
    float scaleGrow;
    float scaleShrink;
};

static const ParticleTypeParam kDefaultParticleTypeParam =
{
    1.00f,
    1.20f,
    0.0f,
    0.6f,
    0.0f,
    0.0f
};

static const uint kParticleTypeParamCount = GPU_PARTICLE_TYPE_BULLET_TRACER + 1;

static const ParticleTypeParam kParticleTypeParams[kParticleTypeParamCount] =
{
    // 0: Default
    { 1.00f, 1.20f, 0.0f, 0.6f, 0.0f, 0.0f },

    // 1: Blood
    { 0.95f, 1.10f, -9.8f, 0.8f, 0.0f, 0.0f },

    // 2: Dust
    { 0.40f, 1.00f, 0.6f, 2.0f, 0.6f, 0.0f },

    // 3: Debris
    { 0.75f, 1.20f, -5.0f, 1.2f, 0.0f, 0.0f },

    // 4: Smoke
    { 0.35f, 0.85f, 0.8f, 1.0f, 0.9f, 0.0f },

    // 5: Ambient
    { 0.12f, 0.60f, 0.0f, 0.2f, 0.0f, 0.0f },

    // 6: Spark
    { 1.00f, 2.20f, -2.0f, 0.6f, 0.0f, 8.0f },

    // 7: Shockwave
    { 0.85f, 1.30f, 0.0f, 0.4f, 0.0f, 0.0f },

    // 8: Heal
    { 0.55f, 1.10f, 0.8f, 0.6f, 0.4f, 0.0f },

    // 9: Trail
    { 0.55f, 1.50f, 0.0f, 0.5f, 0.0f, 0.0f },
    
    // 10: DeathBurstCore
    { 0.95f, 1.80f, 0.4f, 3.8f, 2.8f, 0.0f },

    // 11: PlayerDamageBlood
    { 0.95f, 1.35f, -7.0f, 1.1f, 0.0f, 0.0f },

    // 12: MuzzleFlash
    { 1.00f, 2.80f, 0.0f, 8.0f, 0.0f, 14.0f },

    // 13: BulletTracer
    { 0.85f, 2.20f, 0.0f, 1.2f, 0.0f, 10.0f },
};

ParticleTypeParam GetParticleTypeParam(uint type)
{
    if (type < kParticleTypeParamCount)
    {
        return kParticleTypeParams[type];
    }

    return kDefaultParticleTypeParam;
}