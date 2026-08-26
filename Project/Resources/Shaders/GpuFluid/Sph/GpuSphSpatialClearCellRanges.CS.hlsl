#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= gCellCount)
    {
        return;
    }

    GpuSphCellRange range;
    range.start = kGpuSphInvalidIndex;
    range.count = 0;
    gCellRanges[index] = range;
}
