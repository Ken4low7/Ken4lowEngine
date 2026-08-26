#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 samplePosition = gParticles[index].predictedPosition;
    const int3 baseCell = GpuSphPositionToCell(samplePosition);
    float density = 0.0f;

    // W6では自分のCellと周囲26 Cellだけを調べ、全粒子走査を廃止する。
    [loop]
    for (int z = -1; z <= 1; ++z)
    {
        [loop]
        for (int y = -1; y <= 1; ++y)
        {
            [loop]
            for (int x = -1; x <= 1; ++x)
            {
                const int3 neighborCell = baseCell + int3(x, y, z);
                const GpuSphCellRange range = GpuSphGetCellRange(neighborCell);
                if (range.count == 0 || range.start == kGpuSphInvalidIndex)
                {
                    continue;
                }

                [loop]
                for (uint rangeIndex = 0; rangeIndex < range.count; ++rangeIndex)
                {
                    const uint sortedIndex = range.start + rangeIndex;
                    const uint neighborIndex = gHashEntries[sortedIndex].particleIndex;
                    if (neighborIndex == kGpuSphInvalidIndex || neighborIndex >= gSph.activeParticleCount)
                    {
                        continue;
                    }

                    const float3 neighborPosition = gParticles[neighborIndex].predictedPosition;
                    const float distanceValue = length(samplePosition - neighborPosition);
                    density += gSph.particleMass * GpuSphPoly6Kernel(distanceValue, gSph.smoothingRadius);
                }
            }
        }
    }

    gParticles[index].density = max(density, 1.0e-5f);
}
