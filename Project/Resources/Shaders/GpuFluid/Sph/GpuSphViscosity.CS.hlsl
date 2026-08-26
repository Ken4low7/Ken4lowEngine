#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

[numthreads(128, 1, 1)]
void ComputeViscosityDelta(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionI = gParticles[index].predictedPosition;
    const float3 velocityI = gParticles[index].velocity;
    const float densityI = max(gParticles[index].density, 1.0e-5f);
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float3 acceleration = float3(0.0f, 0.0f, 0.0f);
    float3 xsphDelta = float3(0.0f, 0.0f, 0.0f);

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
                    const float3 velocityJ = gParticles[neighborIndex].velocity;
                    const float neighborVolume = gSph.particleMass / densityJ;

                    const float laplacian = GpuSphViscosityLaplacian(distanceValue, gSph.smoothingRadius);
                    acceleration +=
                        gSph.viscosityStrength * gSph.particleMass *
                        (velocityJ - velocityI) *
                        (laplacian / densityJ);

                    // W9.5: 自由表面のCohesion強度を水Presetから調整可能にする。
                    const float surfaceDensity = min(densityI, densityJ);
                    const float surfaceFactor = saturate(
                        1.0f - surfaceDensity / max(gSph.targetDensity, 1.0e-5f));
                    const float cohesionWeight = GpuSphCohesionWeight(distanceValue, gSph.smoothingRadius);
                    const float cohesionAcceleration =
                        max(gSph.surfaceTensionStrength, 0.0f) *
                        gSph.targetDensity *
                        neighborVolume *
                        cohesionWeight *
                        surfaceFactor;
                    acceleration -= cohesionAcceleration * (delta / distanceValue);

                    xsphDelta +=
                        neighborVolume *
                        (velocityJ - velocityI) *
                        GpuSphPoly6Kernel(distanceValue, gSph.smoothingRadius);
                }
            }
        }
    }

    // 近傍Velocityを読むPassと書くPassを分け、Spatial Hash化後もread/write競合を防ぐ。
    gScratch[index] = float4(
        acceleration * gSph.deltaTime + xsphDelta * clamp(gSph.xsphStrength, 0.0f, 1.0f),
        0.0f);
}

[numthreads(128, 1, 1)]
void ApplyViscosity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 velocityValue = gParticles[index].velocity + gScratch[index].xyz;
    gParticles[index].velocity = GpuSphClampVelocityByCfl(
        velocityValue,
        gSph.smoothingRadius,
        gSph.deltaTime,
        gSph.cflNumber);
}
