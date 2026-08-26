#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint sortedIndex = dispatchThreadId.x;
    if (sortedIndex >= gSph.activeParticleCount)
    {
        return;
    }

    const GpuSphHashEntry entry = gHashEntries[sortedIndex];
    if (entry.key == kGpuSphInvalidIndex || entry.particleIndex == kGpuSphInvalidIndex || entry.key >= gCellCount)
    {
        return;
    }

    uint ignored;
    InterlockedMin(gCellRanges[entry.key].start, sortedIndex, ignored);
    InterlockedAdd(gCellRanges[entry.key].count, 1u, ignored);
}
