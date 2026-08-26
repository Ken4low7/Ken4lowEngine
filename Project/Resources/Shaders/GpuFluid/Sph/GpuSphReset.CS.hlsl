#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const uint xy = max(gSph.spawnDimX * gSph.spawnDimY, 1u);
    const uint x = index % max(gSph.spawnDimX, 1u);
    const uint y = (index / max(gSph.spawnDimX, 1u)) % max(gSph.spawnDimY, 1u);
    const uint z = (index / xy) % max(gSph.spawnDimZ, 1u);

    GpuSphParticle particle = (GpuSphParticle)0;
    particle.position = gSph.spawnOrigin + float3(x, y, z) * gSph.spawnSpacing;
    particle.predictedPosition = particle.position;
    particle.density = gSph.targetDensity;
    gParticles[index] = particle;
    // Viscosity scratchもReset時に明示的にゼロへ戻す。
    gScratch[index] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}
