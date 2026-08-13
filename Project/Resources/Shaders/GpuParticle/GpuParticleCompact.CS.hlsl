#include "GpuParticleData.hlsli"

struct GpuDrivenDraw
{
    uint renderGroup;
    uint primitiveCount;
    uint indexed;
    uint maxParticles;
};

ConstantBuffer<GpuDrivenDraw> gDraw : register(b0);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<uint> gVisibleParticleIndices : register(u1);
RWStructuredBuffer<uint> gIndirectDrawArgs : register(u2);

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint particleIndex = dispatchThreadId.x;

    if (particleIndex == 0u)
    {
        // 5-word buffer is shared by Draw and DrawIndexed signatures; unused words stay zero.
        gIndirectDrawArgs[0] = gDraw.primitiveCount;
        gIndirectDrawArgs[2] = 0u;
        gIndirectDrawArgs[3] = 0u;
        gIndirectDrawArgs[4] = 0u;
    }

    if (particleIndex >= gDraw.maxParticles)
    {
        return;
    }

    Particle particle = gParticles[particleIndex];
    if (particle.lifeTime <= 0.0f || particle.color.a <= 0.0f || particle.type != gDraw.renderGroup)
    {
        return;
    }

    uint compactedIndex = 0u;
    InterlockedAdd(gIndirectDrawArgs[1], 1u, compactedIndex);
    if (compactedIndex < gDraw.maxParticles)
    {
        gVisibleParticleIndices[compactedIndex] = particleIndex;
    }
}
