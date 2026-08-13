#include "GpuParticleData.hlsli"
#include "GpuParticleTypeParams.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b2);

float SampleScalarLut(float4 lut, float t)
{
    float position = saturate(t) * 3.0f;
    uint index = min((uint) floor(position), 2u);
    float localT = frac(position);
    float a = index == 0u ? lut.x : (index == 1u ? lut.y : lut.z);
    float b = index == 0u ? lut.y : (index == 1u ? lut.z : lut.w);
    return lerp(a, b, localT);
}

float4 SampleColorGradient(Particle p, float t)
{
    float position = saturate(t) * 3.0f;
    uint index = min((uint) floor(position), 2u);
    float localT = frac(position);
    float4 a = index == 0u ? p.colorGradientLut0 : (index == 1u ? p.colorGradientLut1 : p.colorGradientLut2);
    float4 b = index == 0u ? p.colorGradientLut1 : (index == 1u ? p.colorGradientLut2 : p.colorGradientLut3);
    return lerp(a, b, localT);
}

float3 EvaluateNoiseAcceleration(Particle p)
{
    if (abs(p.noiseStrength) <= 1e-6f || p.noiseFrequency <= 1e-6f)
        return 0.0f;

    // 時間と位置を格子化した決定的Noiseで、追加Textureなしでも煙や魔法粒子に揺らぎを与える。
    float3 cell = floor((p.translate + gPerFrame.time.xxx) * p.noiseFrequency);
    return (GPURand3(cell + 17.37f) * 2.0f - 1.0f) * p.noiseStrength;
}

float3 EvaluateVortexAcceleration(Particle p)
{
    if (abs(p.vortexStrength) <= 1e-6f)
        return 0.0f;

    float axisLength = length(p.vortexAxis);
    float3 relative = p.translate - p.forceOrigin;
    if (axisLength <= 1e-6f || length(relative) <= 1e-6f)
        return 0.0f;

    float3 tangent = cross(p.vortexAxis / axisLength, relative);
    float tangentLength = length(tangent);
    return tangentLength > 1e-6f ? tangent / tangentLength * p.vortexStrength : 0.0f;
}

float3 EvaluateAttractorAcceleration(Particle p)
{
    if (abs(p.attractorStrength) <= 1e-6f)
        return 0.0f;

    float3 delta = p.attractorPosition - p.translate;
    float distanceToAttractor = length(delta);
    if (distanceToAttractor <= 1e-6f)
        return 0.0f;
    if (p.attractorRadius > 0.0f && distanceToAttractor > p.attractorRadius)
        return 0.0f;

    float falloff = p.attractorRadius > 0.0f
        ? saturate(1.0f - distanceToAttractor / max(p.attractorRadius, 1e-5f))
        : 1.0f;
    return delta / distanceToAttractor * p.attractorStrength * falloff;
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticleCount)
        return;

    Particle p = gParticles[particleIndex];

    if (p.lifeTime <= 0.0f)
        return;

    float dt = gPerFrame.deltaTime;
    p.currentTime += dt;

    if (p.currentTime >= p.lifeTime)
    {
        p.color.a = 0.0f;
        p.scale = float3(0, 0, 0);
        p.lifeTime = 0.0f;
        p.currentTime = 0.0f;

        gParticles[particleIndex] = p;

        int oldTop;
        InterlockedAdd(gFreeListIndex[0], 1, oldTop);
        uint newTop = (uint) (oldTop + 1);

        if (newTop < kMaxParticleCount)
        {
            gFreeList[newTop] = particleIndex;
        }
        else
        {
            InterlockedAdd(gFreeListIndex[0], -1, oldTop);
        }
        return;
    }

    float t = saturate(p.currentTime / max(p.lifeTime, 1e-5f));
    if ((p.customFlags & GPU_PARTICLE_CUSTOM_DESC_OVERRIDE) != 0u)
    {
        float damp = max(0.0f, 1.0f - p.damping * dt);
        p.velocity *= damp;
        float3 acceleration = p.gravity;
        acceleration += EvaluateNoiseAcceleration(p);
        acceleration += EvaluateVortexAcceleration(p);
        acceleration += EvaluateAttractorAcceleration(p);
        p.velocity += acceleration * dt;
        p.translate += p.velocity * dt;

        float sizeMultiplier = (p.customFlags & GPU_PARTICLE_CUSTOM_SIZE_CURVE) != 0u
            ? max(0.0f, SampleScalarLut(p.sizeCurveLut, t))
            : 1.0f;
        p.scale = lerp(p.startScale, p.endScale, t) * sizeMultiplier;

        if ((p.customFlags & GPU_PARTICLE_CUSTOM_COLOR_GRADIENT) != 0u)
        {
            p.color = saturate(SampleColorGradient(p, t));
        }
        else
        {
            p.color = lerp(p.startColor, p.endColor, t);
            if ((p.customFlags & GPU_PARTICLE_CUSTOM_ALPHA_FADE) == 0u)
            {
                p.color.a = p.startColor.a;
            }
        }

        p.rotation += p.rotationSpeed * dt;
        p.rotation3D += p.angularVelocity3D * dt;
    }
    else
    {
        ParticleTypeParam param = GetParticleTypeParam(p.type);

        float damp = max(0.0f, 1.0f - param.drag * dt);
        p.velocity *= damp;
        p.velocity.y += param.accelY * dt;

        p.translate += p.velocity * dt;

        if (param.scaleGrow > 0.0f)
        {
            uint kind = GPUParticle_GetKind(p.billboardMode);
            if (kind == GPU_PARTICLE_KIND_SPRITE || kind == GPU_PARTICLE_KIND_RIBBON)
            {
                p.scale.xy += param.scaleGrow * dt;
            }
            else
            {
                p.scale += param.scaleGrow * dt;
            }
        }

        if (param.scaleShrink > 0.0f)
        {
            float s = max(0.0f, 1.0f - param.scaleShrink * dt);
            p.scale *= s;
        }

        float a = pow(1.0f - t, param.alphaPow) * param.baseAlpha;
        p.color.a = saturate(a);
    }

    gParticles[particleIndex] = p;
}
