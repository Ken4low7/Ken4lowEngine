#include "GpuSphCommon.hlsli"

bool GpuSphEntryGreater(GpuSphHashEntry a, GpuSphHashEntry b)
{
    if (a.key != b.key)
    {
        return a.key > b.key;
    }
    return a.particleIndex > b.particleIndex;
}

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= gSortCount)
    {
        return;
    }

    const uint partnerIndex = index ^ gSortLevelMask;
    if (partnerIndex <= index || partnerIndex >= gSortCount)
    {
        return;
    }

    const GpuSphHashEntry a = gHashEntries[index];
    const GpuSphHashEntry b = gHashEntries[partnerIndex];
    const bool ascending = (index & gSortLevel) == 0;
    const bool shouldSwap = ascending ? GpuSphEntryGreater(a, b) : GpuSphEntryGreater(b, a);
    if (shouldSwap)
    {
        gHashEntries[index] = b;
        gHashEntries[partnerIndex] = a;
    }
}
