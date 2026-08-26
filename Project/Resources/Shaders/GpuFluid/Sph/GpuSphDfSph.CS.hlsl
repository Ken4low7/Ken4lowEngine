#include "GpuSphCommon.hlsli"
#include "GpuSphKernel.hlsli"

float3 DfSphConstraintGradient(float3 delta)
{
    return (gSph.particleMass / max(gSph.targetDensity, 1.0e-5f)) *
        GpuSphSpikyGradient(delta, gSph.smoothingRadius);
}

float3 ClampCorrection(float3 correction)
{
    const float maxCorrection = max(gSph.maxDfsphVelocityCorrection, 0.01f);
    const float lengthValue = length(correction);
    if (lengthValue <= maxCorrection || lengthValue <= 1.0e-6f)
    {
        return correction;
    }
    return correction * (maxCorrection / lengthValue);
}

[numthreads(128, 1, 1)]
void ComputeFactor(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionI = gParticles[index].predictedPosition;
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float3 gradientI = float3(0.0f, 0.0f, 0.0f);
    float gradientSquaredSum = 0.0f;

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

                    const float3 delta = positionI - gParticles[neighborIndex].predictedPosition;
                    const float distanceValue = length(delta);
                    if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
                    {
                        continue;
                    }

                    const float3 gradientJ = DfSphConstraintGradient(delta);
                    gradientI += gradientJ;
                    gradientSquaredSum += dot(gradientJ, gradientJ);
                }
            }
        }
    }

    gradientSquaredSum += dot(gradientI, gradientI);
    // DFSPH係数は同じSpatial Hash近傍を使い、各反復で再計算せず再利用する。
    const float factor = 1.0f / max(gradientSquaredSum, 1.0e-6f);
    gScratch[index] = float4(factor, 0.0f, 0.0f, 0.0f);
}

[numthreads(128, 1, 1)]
void PrepareDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const GpuSphParticle particleI = gParticles[index];
    const float3 positionI = particleI.predictedPosition;
    const float3 velocityI = particleI.velocity;
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float densityRate = 0.0f;

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

                    const GpuSphParticle particleJ = gParticles[neighborIndex];
                    const float3 delta = positionI - particleJ.predictedPosition;
                    const float distanceValue = length(delta);
                    if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
                    {
                        continue;
                    }

                    densityRate += gSph.particleMass * dot(
                        velocityI - particleJ.velocity,
                        GpuSphSpikyGradient(delta, gSph.smoothingRadius));
                }
            }
        }
    }

    const float targetDensity = max(gSph.targetDensity, 1.0e-5f);
    const float predictedDensity = max(particleI.density + gSph.deltaTime * densityRate, 0.0f);
    const float densityError = max(predictedDensity / targetDensity - 1.0f, 0.0f);
    float4 solverState = gDfSphState[index];
    float kappa = 0.0f;
    if (densityError > gSph.dfsphDensityErrorTolerance)
    {
        const float dt2 = max(gSph.deltaTime * gSph.deltaTime, 1.0e-8f);
        kappa = densityError * gScratch[index].x * max(gSph.dfsphDensityRelaxation, 0.0f) / dt2;
        if (gSph.dfsphWarmStartEnabled != 0u && gSortLevel == 0u)
        {
            kappa += max(solverState.x, 0.0f) * saturate(gSph.dfsphWarmStartStrength);
        }
    }

    solverState.x = kappa;
    solverState.z = densityError;
    gDfSphState[index] = solverState;
    gParticles[index].pressure = kappa;
    gParticles[index].padding = densityError;
}

[numthreads(128, 1, 1)]
void ApplyDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionI = gParticles[index].predictedPosition;
    const float kappaI = max(gParticles[index].pressure, 0.0f);
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float3 correction = float3(0.0f, 0.0f, 0.0f);

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

                    const float3 delta = positionI - gParticles[neighborIndex].predictedPosition;
                    const float distanceValue = length(delta);
                    if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
                    {
                        continue;
                    }

                    const float kappaJ = max(gParticles[neighborIndex].pressure, 0.0f);
                    correction -= gSph.deltaTime * (kappaI + kappaJ) * DfSphConstraintGradient(delta);
                }
            }
        }
    }

    gParticles[index].velocity += ClampCorrection(correction);
}

[numthreads(128, 1, 1)]
void PrepareDivergence(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const GpuSphParticle particleI = gParticles[index];
    const float3 positionI = particleI.predictedPosition;
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float divergence = 0.0f;

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

                    const GpuSphParticle particleJ = gParticles[neighborIndex];
                    const float3 delta = positionI - particleJ.predictedPosition;
                    const float distanceValue = length(delta);
                    if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
                    {
                        continue;
                    }

                    divergence += dot(
                        particleI.velocity - particleJ.velocity,
                        DfSphConstraintGradient(delta));
                }
            }
        }
    }

    const float compression = max(divergence, 0.0f);
    float4 solverState = gDfSphState[index];
    float kappa = 0.0f;
    if (compression > gSph.dfsphDivergenceErrorTolerance)
    {
        kappa = compression * gScratch[index].x * max(gSph.dfsphDivergenceRelaxation, 0.0f) /
            max(gSph.deltaTime, 1.0e-6f);
        if (gSph.dfsphWarmStartEnabled != 0u && gSortLevel == 0u)
        {
            kappa += max(solverState.y, 0.0f) * saturate(gSph.dfsphWarmStartStrength);
        }
    }

    solverState.y = kappa;
    solverState.w = compression;
    gDfSphState[index] = solverState;
    gParticles[index].pressure = kappa;
    gParticles[index].padding = compression;
}

[numthreads(128, 1, 1)]
void ApplyDivergence(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const float3 positionI = gParticles[index].predictedPosition;
    const float kappaI = max(gParticles[index].pressure, 0.0f);
    const int3 baseCell = GpuSphPositionToCell(positionI);
    float3 correction = float3(0.0f, 0.0f, 0.0f);

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

                    const float3 delta = positionI - gParticles[neighborIndex].predictedPosition;
                    const float distanceValue = length(delta);
                    if (distanceValue <= 1.0e-6f || distanceValue >= gSph.smoothingRadius)
                    {
                        continue;
                    }

                    const float kappaJ = max(gParticles[neighborIndex].pressure, 0.0f);
                    correction -= gSph.deltaTime * (kappaI + kappaJ) * DfSphConstraintGradient(delta);
                }
            }
        }
    }

    gParticles[index].velocity += ClampCorrection(correction);
}
