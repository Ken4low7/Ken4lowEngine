#include "GpuSphCommon.hlsli"

uint ParticleHash(uint value)
{
    value ^= value >> 16u;
    value *= 0x7feb352du;
    value ^= value >> 15u;
    value *= 0x846ca68bu;
    value ^= value >> 16u;
    return value;
}

float ParticleHash01(uint value)
{
    return float(ParticleHash(value) & 0x00ffffffu) / 16777215.0f;
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

    const float3 randomVector = float3(
        ParticleHash01(index * 3u + 11u),
        ParticleHash01(index * 3u + 23u),
        ParticleHash01(index * 3u + 47u)) * 2.0f - 1.0f;

    GpuSphParticle particle = (GpuSphParticle)0;
    particle.position = gSph.spawnOrigin + float3(x, y, z) * gSph.spawnSpacing;
    particle.position += randomVector * (gSph.spawnSpacing * 0.08f); // 決定論的な小さな揺らぎで格子模様を崩しつつ再現性を保つ。
    particle.position.y = max(particle.position.y, gSph.boundaryMin.y + gSph.spawnSpacing * 0.10f);
    particle.predictedPosition = particle.position;
    particle.density = gSph.targetDensity;
    gParticles[index] = particle;

    gScratch[index] = float4(0.0f, 0.0f, 0.0f, 0.0f);
    gDfSphState[index] = float4(0.0f, 0.0f, 0.0f, 0.0f); // 粘性用scratchとDFSPHの前フレーム状態を同時に初期化する。
}
