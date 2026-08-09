#include "GpuParticleData.hlsli"
#include "GpuParticleTypeParams.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b2);

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
		// Desc Previewは指定velocityで移動し、寿命比でsize/colorを開始値から終了値へ補間する。
        float damp = max(0.0f, 1.0f - p.damping * dt);
        p.velocity *= damp;
        p.velocity += p.gravity * dt;
        p.translate += p.velocity * dt;
        p.scale = lerp(p.startScale, p.endScale, t);
        p.color = lerp(p.startColor, p.endColor, t);
        if ((p.customFlags & GPU_PARTICLE_CUSTOM_ALPHA_FADE) == 0u)
        {
            p.color.a = p.startColor.a;
        }
        p.rotation += p.rotationSpeed * dt;
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
