#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void Predict(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionValue = gParticles[index].position;
    const float3 velocityValue = gParticles[index].velocity;
    gParticles[index].predictedPosition = positionValue + velocityValue * gSph.deltaTime;
}

[numthreads(128, 1, 1)]
void Integrate(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 velocityValue = gParticles[index].velocity;
    const float3 positionValue = gParticles[index].position + velocityValue * gSph.deltaTime;
    // W5.8では最終速度から位置を進め、次Step用の予測位置も更新する。
    gParticles[index].position = positionValue;
    gParticles[index].predictedPosition = positionValue + velocityValue * gSph.deltaTime;
}
