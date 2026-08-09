#include "GpuParticleData.hlsli"

static const uint GPU_PARTICLE_SPAWN_SHAPE_POINT = 0;
static const uint GPU_PARTICLE_SPAWN_SHAPE_SPHERE = 1;
static const uint GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE = 2;
static const uint GPU_PARTICLE_SPAWN_SHAPE_RING = 3;

static const uint GPU_PARTICLE_DIR_RANDOM = 0;
static const uint GPU_PARTICLE_DIR_UPHEMI = 1;
static const uint GPU_PARTICLE_DIR_RADIAL = 2;
static const uint GPU_PARTICLE_DIR_TANGENT = 3;
static const uint GPU_PARTICLE_DIR_UP = 4;

static const uint GPU_PARTICLE_SCALE_UNIFORM = 0;
static const uint GPU_PARTICLE_SCALE_STRETCH = 1;

struct ParticleSpawnParam
{
    float lifeMin;
    float lifeMax;

    float speedMin;
    float speedMax;

    float scaleMin;
    float scaleMax;

    float alpha;

    float3 colorA;
    float3 colorB;

    uint spawnShape;
    uint dirType;
    uint scaleMode;

    float stretchScaleMin;
    float stretchScaleMax;
};

static const ParticleSpawnParam kDefaultSpawnParam =
{
    1.0f, 1.0f,
    2.0f, 2.0f,
    0.08f, 0.08f,
    1.0f,
    float3(1.0f, 1.0f, 1.0f),
    float3(1.0f, 1.0f, 1.0f),
    GPU_PARTICLE_SPAWN_SHAPE_SPHERE,
    GPU_PARTICLE_DIR_RANDOM,
    GPU_PARTICLE_SCALE_UNIFORM,
    1.0f, 1.0f
};

static const uint kParticleSpawnParamCount = GPU_PARTICLE_TYPE_BULLET_TRACER + 1;

static const ParticleSpawnParam kParticleSpawnParams[kParticleSpawnParamCount] =
{
    // 0: Default
    {
        1.0f, 1.0f,
        2.0f, 2.0f,
        0.08f, 0.08f,
        1.0f,
        float3(1.0f, 1.0f, 1.0f),
        float3(1.0f, 1.0f, 1.0f),
        GPU_PARTICLE_SPAWN_SHAPE_SPHERE,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 1: Blood
    {
        0.25f, 0.80f,
        2.0f, 10.0f,
        0.03f, 0.09f,
        0.90f,
        float3(0.45f, 0.03f, 0.03f),
        float3(0.75f, 0.08f, 0.08f),
        GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE,
        GPU_PARTICLE_DIR_UPHEMI,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 2: Dust
    {
        0.35f, 1.20f,
        0.3f, 2.4f,
        0.05f, 0.18f,
        0.40f,
        float3(0.45f, 0.45f, 0.45f),
        float3(0.70f, 0.70f, 0.70f),
        GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE,
        GPU_PARTICLE_DIR_UPHEMI,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 3: Debris
    {
        0.30f, 1.00f,
        1.5f, 7.0f,
        0.03f, 0.10f,
        0.70f,
        float3(0.35f, 0.30f, 0.25f),
        float3(0.60f, 0.60f, 0.60f),
        GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 4: Smoke
    {
        1.00f, 3.00f,
        0.1f, 1.2f,
        0.15f, 0.50f,
        0.35f,
        float3(0.18f, 0.18f, 0.18f),
        float3(0.35f, 0.35f, 0.35f),
        GPU_PARTICLE_SPAWN_SHAPE_SPHERE,
        GPU_PARTICLE_DIR_UPHEMI,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 5: Ambient
    {
        2.0f, 7.0f,
        0.02f, 0.12f,
        0.01f, 0.03f,
        0.12f,
        float3(0.75f, 0.75f, 0.75f),
        float3(0.95f, 0.95f, 0.95f),
        GPU_PARTICLE_SPAWN_SHAPE_SPHERE,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 6: Spark
    {
        0.08f, 0.22f,
        4.0f, 18.0f,
        0.015f, 0.035f,
        1.00f,
        float3(1.0f, 0.85f, 0.50f),
        float3(1.0f, 1.0f, 0.75f),
        GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 7: Shockwave
    {
        0.18f, 0.50f,
        6.0f, 16.0f,
        0.04f, 0.10f,
        0.85f,
        float3(1.0f, 1.0f, 1.0f),
        float3(1.0f, 1.0f, 1.0f),
        GPU_PARTICLE_SPAWN_SHAPE_RING,
        GPU_PARTICLE_DIR_RADIAL,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 8: Heal
    {
        0.50f, 1.40f,
        0.2f, 1.0f,
        0.05f, 0.12f,
        0.55f,
        float3(0.15f, 0.9f, 0.35f),
        float3(0.55f, 1.0f, 0.75f),
        GPU_PARTICLE_SPAWN_SHAPE_RING,
        GPU_PARTICLE_DIR_UP,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 9: Trail
    {
        0.08f, 0.22f,
        0.1f, 0.8f,
        0.04f, 0.08f,
        0.55f,
        float3(0.85f, 0.85f, 0.85f),
        float3(1.0f, 1.0f, 1.0f),
        GPU_PARTICLE_SPAWN_SHAPE_SPHERE,
        GPU_PARTICLE_DIR_TANGENT,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },
    
    // 10: DeathBurstCore
    {
        0.12f, 0.28f,
        5.5f, 10.5f,
        0.06f, 0.16f,
        0.95f,
        float3(1.0f, 1.0f, 1.0f),
        float3(1.0f, 1.0f, 1.0f),
        GPU_PARTICLE_SPAWN_SHAPE_SPHERE,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 11: PlayerDamageBlood
    {
        0.18f, 0.55f,
        1.5f, 7.5f,
        0.025f, 0.075f,
        0.95f,
        float3(0.55f, 0.00f, 0.00f),
        float3(0.95f, 0.04f, 0.04f),
        GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE,
        GPU_PARTICLE_DIR_UPHEMI,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },

    // 12: MuzzleFlash
    {
        0.035f, 0.085f,
        0.6f, 2.2f,
        0.08f, 0.22f,
        1.0f,
        float3(1.0f, 0.75f, 0.20f),
        float3(1.0f, 1.0f, 0.75f),
        GPU_PARTICLE_SPAWN_SHAPE_POINT,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_STRETCH,
        0.16f, 0.36f
    },

    // 13: BulletTracer
    {
        0.16f, 0.30f,
        0.0f, 0.0f,
        0.08f, 0.16f,
        1.0f,
        float3(0.80f, 0.90f, 1.0f),
        float3(1.0f, 1.0f, 1.0f),
        GPU_PARTICLE_SPAWN_SHAPE_POINT,
        GPU_PARTICLE_DIR_RANDOM,
        GPU_PARTICLE_SCALE_UNIFORM,
        1.0f, 1.0f
    },
};

ParticleSpawnParam GetParticleSpawnParam(uint type)
{
    if (type < kParticleSpawnParamCount)
    {
        return kParticleSpawnParams[type];
    }

    return kDefaultSpawnParam;
}