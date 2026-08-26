#include "GpuSphCommon.hlsli"

static const float kGpuSphCflMetricScale = 1000.0f;

[numthreads(1, 1, 1)]
void ClearMetric(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x == 0u)
    {
        // Spatial HashはこのFrameのSPH計算終了後なので、先頭Keyを一時的なmax-speed領域として再利用できる。
        gHashEntries[0].key = 0u;
    }
}

[numthreads(128, 1, 1)]
void MeasureMaxSpeed(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float speed = length(gParticles[index].velocity);
    const uint fixedSpeed = (uint)min(speed * kGpuSphCflMetricScale, 4294960000.0f);
    uint previousValue = 0u;
    InterlockedMax(gHashEntries[0].key, fixedSpeed, previousValue);
}
