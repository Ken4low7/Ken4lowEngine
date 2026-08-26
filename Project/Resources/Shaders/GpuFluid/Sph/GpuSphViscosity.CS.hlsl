#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

[numthreads(128, 1, 1)]
void ComputeViscosityDelta(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const GpuSphParticle particle = gParticles[index];
    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

    for (uint neighborIndex = 0; neighborIndex < gSph.activeParticleCount; ++neighborIndex)
    {
        if (neighborIndex == index)
        {
            continue;
        }

        const GpuSphParticle neighbor = gParticles[neighborIndex];
        const float distanceValue = length(particle.predictedPosition - neighbor.predictedPosition);
        if (distanceValue >= gSph.smoothingRadius)
        {
            continue;
        }

        const float densityJ = max(neighbor.density, 1.0e-5f);
        const float laplacian = GpuSphViscosityLaplacian(distanceValue, gSph.smoothingRadius);
        acceleration +=
            gSph.viscosityStrength * gSph.particleMass *
            (neighbor.velocity - particle.velocity) *
            (laplacian / densityJ);
    }

    // 近傍Velocityを読むPassと書くPassを分け、同一Dispatch内のread/write競合を防ぐ。
    gScratch[index] = float4(acceleration * gSph.deltaTime, 0.0f);
}

[numthreads(128, 1, 1)]
void ApplyViscosity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    particle.velocity += gScratch[index].xyz;
    gParticles[index] = particle;
}
