#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

[numthreads(128, 1, 1)]
void ComputePressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float densityValue = gParticles[index].density;
    // W7ではTait EOSを使い、密度変化へ非線形に反応する弱圧縮性液体として扱う。
    gParticles[index].pressure = GpuSphTaitPressure(
        densityValue,
        gSph.targetDensity,
        gSph.pressureStiffness);
}

[numthreads(128, 1, 1)]
void ApplyPressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionI = gParticles[index].predictedPosition;
    const float densityI = max(gParticles[index].density, 1.0e-5f);
    const float pressureI = gParticles[index].pressure;
    const float3 velocityI = gParticles[index].velocity;
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (int z = -1; z <= 1; ++z)
    {
        [loop]
        for (int y = -1; y <= 1; ++y)
        {
            [loop]
            for (int x = -1; x <= 1; ++x)
            {
                const GpuSphCellRange range = GpuSphGetCellRange(baseCell + int3(x, y, z));
                if (range.count == 0 || range.start == kGpuSphInvalidIndex)
                {
                    continue;
                }

                [loop]
                for (uint rangeIndex = 0; rangeIndex < range.count; ++rangeIndex)
                {
                    const uint neighborIndex = gHashEntries[range.start + rangeIndex].particleIndex;
                    if (neighborIndex == index || neighborIndex == kGpuSphInvalidIndex || neighborIndex >= gSph.activeParticleCount)
                    {
                        continue;
                    }

                    const float3 positionJ = gParticles[neighborIndex].predictedPosition;
                    const float3 delta = positionI - positionJ;
                    const float distanceValue = length(delta);
                    if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
                    {
                        continue;
                    }

                    const float densityJ = max(gParticles[neighborIndex].density, 1.0e-5f);
                    const float pressureJ = gParticles[neighborIndex].pressure;
                    float pressureTerm =
                        pressureI / (densityI * densityI) +
                        pressureJ / (densityJ * densityJ);
                    pressureTerm = GpuSphApplyTensileCorrection(
                        pressureTerm,
                        distanceValue,
                        gSph.smoothingRadius);

                    const float3 gradient = GpuSphSpikyGradient(delta, gSph.smoothingRadius);
                    acceleration -= gSph.particleMass * pressureTerm * gradient;
                }
            }
        }
    }

    gParticles[index].velocity = velocityI + acceleration * gSph.deltaTime;
}
