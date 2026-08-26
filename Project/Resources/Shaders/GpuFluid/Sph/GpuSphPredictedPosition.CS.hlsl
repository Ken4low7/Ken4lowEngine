#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void Predict(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    particle.predictedPosition = particle.position + particle.velocity * gSph.deltaTime;
    gParticles[index] = particle;
}

[numthreads(128, 1, 1)]
void Integrate(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    // W5.8では最終速度から位置を進め、次Step用の予測位置も更新する。
    particle.position += particle.velocity * gSph.deltaTime;
    particle.predictedPosition = particle.position + particle.velocity * gSph.deltaTime;
    gParticles[index] = particle;
}
