#include "GpuParticleData.hlsli"
#include "GpuParticleSpawnParams.hlsli"

// UAVs
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

// CBVs
ConstantBuffer<EmitterCBData> gEmitter : register(b1);
ConstantBuffer<PerFrame> gPerFrame : register(b2);

struct ParticleAnimParam
{
    uint atlasCols;
    uint atlasRows;
    uint animFrameCount;
    float animFps;
    uint animFlags;
    float animSpeed;
};

ParticleAnimParam MakeDefaultAnimParam()
{
    ParticleAnimParam p;
    p.atlasCols = 1;
    p.atlasRows = 1;
    p.animFrameCount = 1;
    p.animFps = 0.0f;
    p.animFlags = 0;
    p.animSpeed = 1.0f;
    return p;
}

ParticleAnimParam GetParticleAnimParam(uint type)
{
    ParticleAnimParam p = MakeDefaultAnimParam();

    switch (type)
    {
        case GPU_PARTICLE_TYPE_SMOKE:
        {
                p.atlasCols = 4;
                p.atlasRows = 4;
                p.animFrameCount = 16;
                p.animFps = 6.0f;
                p.animFlags = GPU_PARTICLE_ANIM_LOOP | GPU_PARTICLE_ANIM_RANDOM_START;
                p.animSpeed = 1.0f;
                break;
            }

        case GPU_PARTICLE_TYPE_HEAL:
        {
                p.atlasCols = 4;
                p.atlasRows = 4;
                p.animFrameCount = 16;
                p.animFps = 12.0f;
                p.animFlags = GPU_PARTICLE_ANIM_LOOP;
                p.animSpeed = 1.0f;
                break;
            }

        case GPU_PARTICLE_TYPE_AMBIENT:
        {
                p.atlasCols = 2;
                p.atlasRows = 2;
                p.animFrameCount = 4;
                p.animFps = 4.0f;
                p.animFlags = GPU_PARTICLE_ANIM_LOOP | GPU_PARTICLE_ANIM_RANDOM_START;
                p.animSpeed = 1.0f;
                break;
            }

        default:
            break;
    }

    return p;
}

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
float3 SafeNormalize3(float3 v, float3 fallbackDir)
{
    float len = length(v);
    return (len > 1e-6f) ? (v / len) : normalize(fallbackDir);
}

float3 SampleUnitDir(float3 seed)
{
    float3 r = GPURand3(seed) * 2.0f - 1.0f;
    return SafeNormalize3(r, float3(0.0f, 1.0f, 0.0f));
}

float3 SampleSphere(float3 seed, float radius)
{
    float3 dir = SampleUnitDir(seed);
    float t = GPURand1(seed + 19.19f);
    float r = radius * pow(t, 1.0f / 3.0f);
    return dir * r;
}

float3 SampleHemisphereUp(float3 seed)
{
    float3 d = SampleUnitDir(seed);
    d.y = abs(d.y);
    return normalize(d);
}

float RandRange(float3 seed, float minV, float maxV)
{
    return lerp(minV, maxV, GPURand1(seed));
}

float3 RandColor(float3 seed, float3 a, float3 b)
{
    return lerp(a, b, GPURand1(seed));
}

uint DecideRenderKind()
{
    // 描画種別は明示的なkindを優先し、billboard=falseのSpriteをMeshへ誤分類しない。
    return GPUParticle_GetKind(gEmitter.billboardMode);
}

void FinalizeParticle(inout Particle p, uint kind)
{
    p.currentTime = 0.0f;
    p.type = gEmitter.type;

    uint bbFlags = GPUParticle_GetBillboardFlags(gEmitter.billboardMode);

    if (kind == GPU_PARTICLE_KIND_RIBBON)
    {
        bbFlags |= BILLBOARD_RIBBON;
        bbFlags |= BILLBOARD_CAMERA;
    }

    p.billboardMode = GPUParticle_PackBillboardMode(kind, bbFlags);
}

float3 SampleSpawnOffset(float3 seed, uint spawnShape, float radius)
{
    switch (spawnShape)
    {
        case GPU_PARTICLE_SPAWN_SHAPE_POINT:
            return float3(0.0f, 0.0f, 0.0f);

        case GPU_PARTICLE_SPAWN_SHAPE_HEMISPHERE:
            return SampleHemisphereUp(seed) * (GPURand1(seed + 3.1f) * radius);

        case GPU_PARTICLE_SPAWN_SHAPE_RING:
    {
                float a = GPURand1(seed + 7.1f) * 6.2831853f;
                float r = radius * (0.10f + GPURand1(seed + 8.2f) * 0.90f);
                return float3(cos(a) * r, 0.0f, sin(a) * r);
            }

        case GPU_PARTICLE_SPAWN_SHAPE_SPHERE:
        default:
            return SampleSphere(seed, radius);
    }
}

float3 SampleSpawnDirection(float3 seed, uint dirType, float3 offset)
{
    switch (dirType)
    {
        case GPU_PARTICLE_DIR_UPHEMI:
            return SampleHemisphereUp(seed);

        case GPU_PARTICLE_DIR_RADIAL:
            return normalize(offset + float3(0.0001f, 0.0001f, 0.0001f));

        case GPU_PARTICLE_DIR_TANGENT:
    {
                float3 radial = normalize(float3(offset.x, 0.0f, offset.z) + float3(0.0001f, 0.0f, 0.0001f));
                return normalize(float3(-radial.z, 0.0f, radial.x));
            }

        case GPU_PARTICLE_DIR_UP:
            return float3(0.0f, 1.0f, 0.0f);

        case GPU_PARTICLE_DIR_RANDOM:
        default:
            return SampleUnitDir(seed);
    }
}

void InitParticleCommon(uint i, float3 seed, ParticleSpawnParam param, inout Particle p)
{
    float3 offset = SampleSpawnOffset(seed, param.spawnShape, gEmitter.radius);
    float3 dir = SampleSpawnDirection(seed + 11.3f, param.dirType, offset);

    // DeathBurstCore 用の方向補正
    if (gEmitter.type == GPU_PARTICLE_TYPE_DEATH_BURST_CORE)
    {
        dir = normalize(dir + float3(0.0f, 0.25f, 0.0f));
    }

    p.translate = gEmitter.translate + offset;
    // エミッター単位の調整倍率で、同じGPUタイプでも攻撃ごとに寿命と初速を変えられるようにする。
    p.velocity = dir * RandRange(seed + 21.7f, param.speedMin, param.speedMax) * max(gEmitter.speedScale, 0.0f);
    p.lifeTime = RandRange(seed + 31.9f, param.lifeMin, param.lifeMax) * max(gEmitter.lifeScale, 0.01f);

    if (param.scaleMode == GPU_PARTICLE_SCALE_STRETCH)
    {
        float w = RandRange(seed + 41.1f, param.scaleMin, param.scaleMax);
        float h = RandRange(seed + 51.2f, param.stretchScaleMin, param.stretchScaleMax);
        p.scale = float3(w, h, 1.0f);
    }
    else
    {
        float s = RandRange(seed + 61.4f, param.scaleMin, param.scaleMax);
        p.scale = float3(s, s, s);
    }

    p.color = float4(RandColor(seed + 71.5f, param.colorA, param.colorB), param.alpha);

    ParticleAnimParam anim = GetParticleAnimParam(gEmitter.type);

    uint maxFrames = max(1u, anim.atlasCols * anim.atlasRows);

    p.atlasCols = max(1u, anim.atlasCols);
    p.atlasRows = max(1u, anim.atlasRows);
    p.animFrameCount = min(max(1u, anim.animFrameCount), maxFrames);
    p.animFps = max(0.0f, anim.animFps);
    p.animFlags = anim.animFlags;
    p.animSpeed = max(0.01f, anim.animSpeed);

    if ((p.animFlags & GPU_PARTICLE_ANIM_RANDOM_START) != 0u)
    {
        p.startFrame = (uint) (GPURand1(seed + 91.7f) * p.animFrameCount);
    }
    else
    {
        p.startFrame = 0;
    }
}

void ApplyParticleSpawnOverride(uint i, float3 seed, inout Particle p)
{
    switch (gEmitter.type)
    {
        case GPU_PARTICLE_TYPE_DEFAULT:
        {
                float3 dir = SampleUnitDir(seed);
                float t = frac((float) i / max(gEmitter.count, 1u) + gPerFrame.time * 0.2f);

                float r = saturate(abs(t * 6.0f - 3.0f) - 1.0f);
                float g = saturate(2.0f - abs(t * 6.0f - 2.0f));
                float b = saturate(2.0f - abs(t * 6.0f - 4.0f));

                p.translate = gEmitter.translate + dir * gEmitter.radius;
                p.scale = float3(0.08f, 0.08f, 0.08f);
                p.velocity = dir * 2.0f;
                p.lifeTime = 1.0f;
                p.color = float4(r, g, b, 1.0f);
                break;
            }

        case GPU_PARTICLE_TYPE_BLOOD:
        {
                float3 dir = SampleHemisphereUp(seed);
                dir = normalize(dir + float3(0.0f, 0.15f, 0.0f));

                float dist = GPURand1(seed + 1.0f) * gEmitter.radius * 0.15f;
                p.translate = gEmitter.translate + dir * dist;

                float speed = 2.0f + GPURand1(seed + 2.0f) * 8.0f;
                p.velocity = dir * speed;

                float s = 0.03f + GPURand1(seed + 3.0f) * 0.05f;
                p.scale = float3(s, s, s);
                break;
            }

        case GPU_PARTICLE_TYPE_DUST:
        {
                float3 offset = SampleSphere(seed + 1.0f, gEmitter.radius * 0.20f);
                offset.y = abs(offset.y) * 0.08f;
                p.translate = gEmitter.translate + offset;

                p.velocity.xz *= 0.5f;
                p.velocity.y = abs(p.velocity.y) + 0.2f;

                float s = 0.06f + GPURand1(seed + 2.0f) * 0.10f;
                p.scale = float3(s, s, s);
                break;
            }

        case GPU_PARTICLE_TYPE_DEBRIS:
        {
                // ロケット着弾用の砂粒・小石。Mesh でも直方体に見えないようXYZ同一スケールにする。
                float3 dir = SampleUnitDir(seed + 1.0f);
                dir.y = abs(dir.y) * 0.45f + 0.12f;
                dir = normalize(dir);

                p.translate = gEmitter.translate + dir * (GPURand1(seed + 2.0f) * gEmitter.radius * 0.06f);
                p.velocity = dir * (5.0f + GPURand1(seed + 3.0f) * 11.0f);
                p.lifeTime = 0.45f + GPURand1(seed + 5.0f) * 0.55f;

                float s = 0.010f + GPURand1(seed + 4.0f) * 0.020f;
                p.scale = float3(s, s, s);
                p.color = float4(
                    lerp(float3(0.45f, 0.36f, 0.24f), float3(0.78f, 0.68f, 0.48f), GPURand1(seed + 6.0f)),
                    0.95f);
                break;
            }

        case GPU_PARTICLE_TYPE_SMOKE:
        {
                float3 offset = SampleSphere(seed, gEmitter.radius);
                p.translate = gEmitter.translate + offset;

                float3 dir = normalize(offset + float3(0.0f, 0.4f, 0.0f));
                float speed = 0.2f + GPURand1(seed + 2.0f) * 0.8f;
                p.velocity = dir * speed;
                break;
            }

        case GPU_PARTICLE_TYPE_AMBIENT:
        {
                p.translate.y = gEmitter.translate.y + GPURand1(seed + 3.0f) * gEmitter.radius;
                p.velocity = float3(
                (GPURand1(seed + 4.0f) - 0.5f) * 0.08f,
                (GPURand1(seed + 5.0f) - 0.5f) * 0.04f,
                (GPURand1(seed + 6.0f) - 0.5f) * 0.08f);
                break;
            }

        case GPU_PARTICLE_TYPE_SPARK:
        {
                float3 dir = SampleUnitDir(seed + 1.0f);
                dir.y = abs(dir.y) * 0.6f + 0.2f;
                dir = normalize(dir);

                p.translate = gEmitter.translate + dir * (GPURand1(seed + 2.0f) * gEmitter.radius * 0.08f);
                p.velocity = dir * (4.0f + GPURand1(seed + 3.0f) * 14.0f);

                float s = 0.015f + GPURand1(seed + 4.0f) * 0.02f;
                p.scale = float3(s, s, s);
                break;
            }

        case GPU_PARTICLE_TYPE_SHOCKWAVE:
        {
                float a = GPURand1(seed + 1.0f) * 6.2831853f;
                float r = gEmitter.radius * (0.75f + GPURand1(seed + 2.0f) * 0.25f);

                float3 radial = normalize(float3(cos(a), 0.0f, sin(a)));
                p.translate = gEmitter.translate + radial * r;
                p.velocity = radial * (1.0f + GPURand1(seed + 3.0f) * 2.5f);

                float s = 0.035f + GPURand1(seed + 4.0f) * 0.035f;
                p.scale = float3(s, s, s);
                break;
            }

        case GPU_PARTICLE_TYPE_HEAL:
        {
                p.velocity = float3(
                p.velocity.x,
                abs(p.velocity.y) + 0.6f + GPURand1(seed + 2.0f) * 0.8f,
                p.velocity.z);

                float s = 0.06f + GPURand1(seed + 3.0f) * 0.06f;
                p.scale = float3(s, s, s);
                break;
            }

        case GPU_PARTICLE_TYPE_TRAIL:
        {
                float3 offset = SampleSphere(seed, gEmitter.radius * 0.35f);
                p.translate = gEmitter.translate + offset;

                float3 dir = SampleUnitDir(seed + 2.0f);
                p.velocity = dir * (0.1f + GPURand1(seed + 3.0f) * 0.8f);

                float s = 0.04f + GPURand1(seed + 4.0f) * 0.04f;
                p.scale = float3(s, s, s);
                break;
            }
        
        case GPU_PARTICLE_TYPE_DEATH_BURST_CORE:
         {
                float3 dir = SampleUnitDir(seed + 1.0f);
                dir.y = abs(dir.y) * 0.7f + 0.3f;
                dir = normalize(dir);
                p.translate = gEmitter.translate + dir * (GPURand1(seed + 2.0f) * gEmitter.radius * 0.05f);
                p.velocity = dir * (5.5f + GPURand1(seed + 3.0f) * 5.0f);
                float s = 0.06f + GPURand1(seed + 4.0f) * 0.10f;
                p.scale = float3(s, s, s);
                break;
            }
    }
}

void SpawnByType(uint i, float3 seed, inout Particle p)
{
    ParticleSpawnParam param = GetParticleSpawnParam(gEmitter.type);
    InitParticleCommon(i, seed, param, p);
    ApplyParticleSpawnOverride(i, seed, p);
}

// ImGuiで編集したEmitter設定を、Preview用GPUパーティクルの生成値へ直接反映する。
// overrideFlagsが無効な既存Emitterは従来のType別生成処理だけを使用する。
void ApplyEmitterDescOverride(float3 seed, uint kind, inout Particle p)
{
    if ((gEmitter.overrideFlags & GPU_PARTICLE_CUSTOM_DESC_OVERRIDE) == 0u)
        return;

    float3 positionRandom = (GPURand3(seed + 101.0f) * 2.0f - 1.0f) * abs(gEmitter.positionRandom);
    if (gEmitter.spawnShape == 1u) // Sphere
    {
        positionRandom += SampleSphere(seed + 111.0f, max(gEmitter.spawnRadius, 0.0f));
    }
    else if (gEmitter.spawnShape == 2u) // Box
    {
        positionRandom += (GPURand3(seed + 121.0f) * 2.0f - 1.0f) * abs(gEmitter.spawnBoxSize) * 0.5f;
    }

    float3 velocityRandom = (GPURand3(seed + 131.0f) * 2.0f - 1.0f) * abs(gEmitter.velocityRandom);
    float3 initialVelocity = gEmitter.velocity + velocityRandom;
    if (gEmitter.speed > 0.0f)
    {
        float speedValue = max(0.0f, gEmitter.speed + (GPURand1(seed + 141.0f) * 2.0f - 1.0f) * abs(gEmitter.speedRandom));
        initialVelocity = SafeNormalize3(initialVelocity, SampleUnitDir(seed + 151.0f)) * speedValue;
    }

    float sizeFactor = max(0.0001f, 1.0f + (GPURand1(seed + 161.0f) * 2.0f - 1.0f) * abs(gEmitter.sizeRandom));
    float3 spriteStartScale = float3(gEmitter.startSize, (gEmitter.startSize.x + gEmitter.startSize.y) * 0.5f);
    float3 spriteEndScale = float3(gEmitter.endSize, (gEmitter.endSize.x + gEmitter.endSize.y) * 0.5f);
    float4 colorRandom = (float4(GPURand3(seed + 171.0f), GPURand1(seed + 181.0f)) * 2.0f - 1.0f) * abs(gEmitter.colorRandom);

    p.translate = gEmitter.translate + positionRandom;
    p.velocity = initialVelocity;
    p.lifeTime = max(0.01f, gEmitter.lifeTime + (GPURand1(seed + 191.0f) * 2.0f - 1.0f) * abs(gEmitter.lifeTimeRandom));
    p.startScale = max(kind == GPU_PARTICLE_KIND_MESH ? gEmitter.startScale3D : spriteStartScale, float3(0.0001f, 0.0001f, 0.0001f)) * sizeFactor;
    p.endScale = max(kind == GPU_PARTICLE_KIND_MESH ? gEmitter.endScale3D : spriteEndScale, float3(0.0001f, 0.0001f, 0.0001f)) * sizeFactor;
    p.scale = p.startScale;
    p.startColor = saturate(gEmitter.startColor + colorRandom);
    p.endColor = saturate(gEmitter.endColor);
    p.color = p.startColor;
    p.customFlags = GPU_PARTICLE_CUSTOM_DESC_OVERRIDE;
    if (gEmitter.alphaFade != 0u) p.customFlags |= GPU_PARTICLE_CUSTOM_ALPHA_FADE;
    p.gravity = gEmitter.gravity;
    p.damping = max(gEmitter.damping, 0.0f);
    p.rotation = gEmitter.startRotation + (GPURand1(seed + 201.0f) * 2.0f - 1.0f) * abs(gEmitter.rotationRandom);
    p.rotationSpeed = gEmitter.rotationSpeed;

    if (gEmitter.useSpriteSheet != 0u)
    {
        p.atlasRows = max(gEmitter.spriteSheetRows, 1u);
        p.atlasCols = max(gEmitter.spriteSheetColumns, 1u);
        p.animFrameCount = p.atlasRows * p.atlasCols;
        p.animFps = max(gEmitter.spriteSheetFrameRate, 0.0f);
        p.animFlags = GPU_PARTICLE_ANIM_LOOP;
    }
}

// ------------------------------------------------------------
// Entry
// ------------------------------------------------------------
[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0 || gEmitter.count == 0)
        return;

    uint kind = DecideRenderKind();

    for (uint i = 0; i < gEmitter.count; ++i)
    {
        int top;
        InterlockedAdd(gFreeListIndex[0], -1, top);

        if (0 <= top && top < (int) kMaxParticleCount)
        {
            uint particleIndex = gFreeList[top];

            Particle p = (Particle) 0;

            float3 seed = float3(
                (float) i * 12.9898f,
                gPerFrame.time * 78.233f,
                (float) gEmitter.type * 37.719f);

            SpawnByType(i, seed, p);
            ApplyEmitterDescOverride(seed, kind, p);
            FinalizeParticle(p, kind);

            gParticles[particleIndex] = p;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], 1, top);
            break;
        }
    }
}
