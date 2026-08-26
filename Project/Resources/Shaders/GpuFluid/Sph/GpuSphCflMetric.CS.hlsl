#include "GpuSphCommon.hlsli"

static const float kGpuSphCflMetricScale = 1000.0f;
static const float kGpuSphConstraintMetricScale = 1000000.0f;

[numthreads(1, 1, 1)]
void ClearMetric(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x == 0u)
    {
        // SPH Step終了後だけHash Buffer先頭16 bytesを一時Metrics領域として再利用する。
        gHashEntries[0].key = 0u;
        gHashEntries[0].particleIndex = 0u;
        gHashEntries[1].key = 0u;
        gHashEntries[1].particleIndex = 0u;
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
    const float4 solverState = gDfSphState[index];
    const float densityError = gSph.dfsphEnabled != 0u ? max(solverState.z, 0.0f) : 0.0f;
    const float divergenceError = gSph.dfsphEnabled != 0u ? max(solverState.w, 0.0f) : 0.0f;

    const uint fixedSpeed = (uint)min(speed * kGpuSphCflMetricScale, 4294960000.0f);
    const uint fixedDensityError = (uint)min(densityError * kGpuSphConstraintMetricScale, 4294960000.0f);
    const uint fixedDivergenceError = (uint)min(divergenceError * kGpuSphConstraintMetricScale, 4294960000.0f);

    uint previousValue = 0u;
    InterlockedMax(gHashEntries[0].key, fixedSpeed, previousValue);
    InterlockedMax(gHashEntries[0].particleIndex, fixedDensityError, previousValue);
    InterlockedMax(gHashEntries[1].key, fixedDivergenceError, previousValue);
}
