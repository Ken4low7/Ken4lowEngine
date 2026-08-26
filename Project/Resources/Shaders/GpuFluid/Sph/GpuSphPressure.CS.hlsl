#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

[numthreads(128, 1, 1)]
void ComputePressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    // 負圧による過剰な引力を避け、W5では圧縮時だけ圧力を発生させる。
    particle.pressure = max(particle.density - gSph.targetDensity, 0.0f) * gSph.pressureStiffness;
    gParticles[index] = particle;
}

[numthreads(128, 1, 1)]
void ApplyPressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    const float densityI = max(particle.density, 1.0e-5f);
    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

    for (uint neighborIndex = 0; neighborIndex < gSph.activeParticleCount; ++neighborIndex)
    {
        if (neighborIndex == index)
        {
            continue;
        }

        const GpuSphParticle neighbor = gParticles[neighborIndex];
        const float3 delta = particle.predictedPosition - neighbor.predictedPosition;
        const float distanceValue = length(delta);
        if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
        {
            continue;
        }

        const float densityJ = max(neighbor.density, 1.0e-5f);
        const float pressureTerm =
            particle.pressure / (densityI * densityI) +
            neighbor.pressure / (densityJ * densityJ);
        const float3 gradient = GpuSphSpikyGradient(delta, gSph.smoothingRadius);
        acceleration -= gSph.particleMass * pressureTerm * gradient;
    }

    // W5.6はPressure accelerationを速度へ積分する。
    particle.velocity += acceleration * gSph.deltaTime;
    gParticles[index] = particle;
}
