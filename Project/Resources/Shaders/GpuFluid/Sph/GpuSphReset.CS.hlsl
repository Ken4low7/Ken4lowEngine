#include "GpuSphCommon.hlsli"

uint W85Hash(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float W85Hash01(uint value)
{
    return float(W85Hash(value) & 0x00ffffffu) / 16777215.0f;
}

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    const uint xy = max(gSph.spawnDimX * gSph.spawnDimY, 1u);
    const uint x = index % max(gSph.spawnDimX, 1u);
    const uint y = (index / max(gSph.spawnDimX, 1u)) % max(gSph.spawnDimY, 1u);
    const uint z = (index / xy) % max(gSph.spawnDimZ, 1u);

    const float3 basePosition = gSph.spawnOrigin + float3(x, y, z) * gSph.spawnSpacing;
    const float3 randomVector = float3(
        W85Hash01(index * 3u + 11u),
        W85Hash01(index * 3u + 23u),
        W85Hash01(index * 3u + 47u)) * 2.0f - 1.0f;

    // W8.5: 小さな決定論的Jitterで完全な格子模様だけを崩し、Reset結果の再現性は維持する。
    const float jitterAmount = gSph.spawnSpacing * 0.08f;
    float3 jitteredPosition = basePosition + randomVector * jitterAmount;
    jitteredPosition.y = max(jitteredPosition.y, gSph.boundaryMin.y + gSph.spawnSpacing * 0.10f);

    GpuSphParticle particle = (GpuSphParticle)0;
    particle.position = jitteredPosition;
    particle.predictedPosition = particle.position;
    particle.density = gSph.targetDensity;
    gParticles[index] = particle;
    // Viscosity scratchもReset時に明示的にゼロへ戻す。
    gScratch[index] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}
