#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= gSortCount)
    {
        return;
    }

    GpuSphHashEntry entry;
    entry.key = kGpuSphInvalidIndex;
    entry.particleIndex = kGpuSphInvalidIndex;

    if (index < gSph.activeParticleCount)
    {
        const int3 cell = GpuSphPositionToCell(gParticles[index].predictedPosition);
        if (GpuSphIsCellValid(cell))
        {
            const uint key = GpuSphCellToKey(cell);
            if (key < gSph.spatialCellCount)
            {
                entry.key = key;
                entry.particleIndex = index;
            }
        }
    }

    // Bitonic Sort用のPadding要素はUINT_MAXへ送り、有効Entryより必ず後ろへ並べる。
    gHashEntries[index] = entry;
}
