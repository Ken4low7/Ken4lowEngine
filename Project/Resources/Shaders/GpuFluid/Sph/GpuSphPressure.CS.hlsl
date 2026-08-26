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

    const float densityValue = gParticles[index].density;
    // 負圧による過剰な引力を避け、W5では圧縮時だけ圧力を発生させる。
    gParticles[index].pressure = max(densityValue - gSph.targetDensity, 0.0f) * gSph.pressureStiffness;
}

[numthreads(128, 1, 1)]
void ApplyPressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionI = gParticles[index].predictedPosition;
    const float densityI = max(gParticles[index].density, 1.0e-5f);
    const float pressureI = gParticles[index].pressure;
    const float3 velocityI = gParticles[index].velocity;
    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

    for (uint neighborIndex = 0; neighborIndex < gSph.activeParticleCount; ++neighborIndex)
    {
        if (neighborIndex == index)
        {
            continue;
        }

        const float3 positionJ = gParticles[neighborIndex].predictedPosition;
        const float3 delta = positionI - positionJ;
        const float distanceValue = length(delta);
        if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
        {
            continue;
        }

        const float densityJ = max(gParticles[neighborIndex].density, 1.0e-5f);
        const float pressureJ = gParticles[neighborIndex].pressure;
        const float pressureTerm =
            pressureI / (densityI * densityI) +
            pressureJ / (densityJ * densityJ);
        const float3 gradient = GpuSphSpikyGradient(delta, gSph.smoothingRadius);
        acceleration -= gSph.particleMass * pressureTerm * gradient;
    }

    // W5.6はPressure accelerationを速度へ積分し、近傍が読む圧力/位置には触れない。
    gParticles[index].velocity = velocityI + acceleration * gSph.deltaTime;
}
