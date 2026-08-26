#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 samplePosition = gParticles[index].predictedPosition;
    float density = 0.0f;

    // W5.5はW6のSpatial Hash導入前なので全粒子を走査して密度を求める。
    for (uint neighborIndex = 0; neighborIndex < gSph.activeParticleCount; ++neighborIndex)
    {
        const float distanceValue = length(samplePosition - gParticles[neighborIndex].predictedPosition);
        density += gSph.particleMass * GpuSphPoly6Kernel(distanceValue, gSph.smoothingRadius);
    }

    GpuSphParticle particle = gParticles[index];
    particle.density = max(density, 1.0e-5f);
    gParticles[index] = particle;
}
