#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    // W5.2では外力として重力だけを速度へ積分する。
    particle.velocity += gSph.gravity * gSph.deltaTime;
    gParticles[index] = particle;
}
