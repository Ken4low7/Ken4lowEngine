#pragma once

/// ---------- Config ---------- ///
static const uint kMaxParticleCount = 131072;

/// ---------- 描画種類 ---------- ///
static const uint GPU_PARTICLE_KIND_SHIFT = 16;
static const uint GPU_PARTICLE_KIND_MASK = 0x00FF0000;
static const uint GPU_PARTICLE_BB_MASK = 0x0000FFFF;

static const uint GPU_PARTICLE_KIND_SPRITE = 0;
static const uint GPU_PARTICLE_KIND_MESH = 1;
static const uint GPU_PARTICLE_KIND_RIBBON = 2;
static const uint GPU_PARTICLE_KIND_BEAM = 3;

/// ---------- ビルボードフラグ ---------- ///
static const uint BILLBOARD_NONE = 0;
static const uint BILLBOARD_CAMERA = 1u << 0;
static const uint BILLBOARD_YAXIS = 1u << 1;
static const uint BILLBOARD_RIBBON = 1u << 2;

static const uint GPU_PARTICLE_ANIM_LOOP = 1u << 0;
static const uint GPU_PARTICLE_ANIM_RANDOM_START = 1u << 1;
static const uint GPU_PARTICLE_CUSTOM_DESC_OVERRIDE = 1u << 0;
static const uint GPU_PARTICLE_CUSTOM_ALPHA_FADE = 1u << 1;

uint GPUParticle_GetKind(uint packedBillboardMode)
{
    return (packedBillboardMode & GPU_PARTICLE_KIND_MASK) >> GPU_PARTICLE_KIND_SHIFT;
}

uint GPUParticle_GetBillboardFlags(uint packedBillboardMode)
{
    return (packedBillboardMode & GPU_PARTICLE_BB_MASK);
}

uint GPUParticle_PackBillboardMode(uint kind, uint billboardFlags)
{
    return ((kind << GPU_PARTICLE_KIND_SHIFT) & GPU_PARTICLE_KIND_MASK) |
           (billboardFlags & GPU_PARTICLE_BB_MASK);
}

/// ---------- パーティクルタイプ ---------- ///
static const uint GPU_PARTICLE_TYPE_DEFAULT = 0;
static const uint GPU_PARTICLE_TYPE_BLOOD = 1;
static const uint GPU_PARTICLE_TYPE_DUST = 2;
static const uint GPU_PARTICLE_TYPE_DEBRIS = 3;
static const uint GPU_PARTICLE_TYPE_SMOKE = 4;
static const uint GPU_PARTICLE_TYPE_AMBIENT = 5;
static const uint GPU_PARTICLE_TYPE_SPARK = 6;
static const uint GPU_PARTICLE_TYPE_SHOCKWAVE = 7;
static const uint GPU_PARTICLE_TYPE_HEAL = 8;
static const uint GPU_PARTICLE_TYPE_TRAIL = 9;
static const uint GPU_PARTICLE_TYPE_DEATH_BURST_CORE = 10;
static const uint GPU_PARTICLE_TYPE_PLAYER_DAMAGE_BLOOD = 11;
static const uint GPU_PARTICLE_TYPE_MUZZLE_FLASH = 12;
static const uint GPU_PARTICLE_TYPE_BULLET_TRACER = 13;

/// ---------- パーティクルデータ ---------- ///
struct Particle
{
    float3 translate;
    float _pad0;

    float3 scale;
    float lifeTime;

    float3 velocity;
    float currentTime;

    uint type;
    uint billboardMode;
    uint atlasCols;
    uint atlasRows;

    uint animFrameCount;
    float animFps;
    uint animFlags;
    uint startFrame;

    float animSpeed;
    float3 _pad1;

    float4 color;

    float3 startScale;
    uint customFlags;
    float3 endScale;
    float customPadding;
    float4 startColor;
    float4 endColor;
    float3 gravity;
    float damping;
    float rotation;
    float rotationSpeed;
    float2 customPadding2;
};

/// ---------- エミッター ---------- ///
struct EmitterCBData
{
    float3 translate;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
    uint type;
    uint billboardMode;
    float lifeScale;
    float speedScale;
    uint overrideFlags;
    uint maxParticles;
    float2 overridePadding;
    float3 positionRandom;
    float lifeTime;
    float3 velocity;
    float lifeTimeRandom;
    float3 velocityRandom;
    float sizeRandom;
    float2 startSize;
    float2 endSize;
    float4 startColor;
    float4 endColor;
    float3 gravity;
    float damping;
    float speed;
    float speedRandom;
    float startRotation;
    float rotationSpeed;
    float rotationRandom;
    float spawnRadius;
    uint spawnShape;
    uint alphaFade;
    float3 spawnBoxSize;
    float spawnBoxPadding;
    float4 colorRandom;
    float3 startScale3D;
    float startScalePadding;
    float3 endScale3D;
    float endScalePadding;
    uint useSpriteSheet;
    uint spriteSheetRows;
    uint spriteSheetColumns;
    float spriteSheetFrameRate;
};

/// ---------- 時間制御 ---------- ///
struct PerFrame
{
    float time;
    float deltaTime;
};

float3 GPURand3(float3 seed)
{
    seed = frac(seed * 0.1031f);
    seed += dot(seed, seed.yzx + 33.33f);
    return frac((seed.xxy + seed.yzz) * seed.zyx);
}

float GPURand1(float3 seed)
{
    return GPURand3(seed).x;
}
