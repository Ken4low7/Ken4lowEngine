#include "GpuParticleData.hlsli"

struct PerView
{
    float4x4 viewProjectionMatrix;
    float4x4 billboardMatrix;
    uint billboardMode;
    float3 padding;
};

struct SortConstants
{
    uint sortLevel;
    uint sortLevelMask;
    uint maxParticles;
    uint padding;
};

ConstantBuffer<PerView> gPerView : register(b0);
ConstantBuffer<SortConstants> gSort : register(b1);
RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<uint> gVisibleParticleIndices : register(u1);
RWStructuredBuffer<uint> gIndirectDrawArgs : register(u2);

static const uint GPU_PARTICLE_INVALID_INDEX = 0xFFFFFFFFu;
static const float GPU_PARTICLE_SORT_SENTINEL = 3.402823466e+38f;

uint ResolveSortCapacity()
{
    uint visibleCount = min(gIndirectDrawArgs[1], gSort.maxParticles);
    if (visibleCount <= 1u)
    {
        return 1u;
    }

    // Visible countの次の2冪だけを有効networkにして、疎なAlpha Effectで無駄なcompareを抑える。
    uint value = visibleCount - 1u;
    value |= value >> 1u;
    value |= value >> 2u;
    value |= value >> 4u;
    value |= value >> 8u;
    value |= value >> 16u;
    return min(value + 1u, gSort.maxParticles);
}

float ResolveSortKey(uint particleIndex)
{
    if (particleIndex == GPU_PARTICLE_INVALID_INDEX)
    {
        return GPU_PARTICLE_SORT_SENTINEL;
    }

    float4 clipPosition = mul(float4(gParticles[particleIndex].translate, 1.0f), gPerView.viewProjectionMatrix);
    if (clipPosition.w <= 1e-6f)
    {
        return GPU_PARTICLE_SORT_SENTINEL * 0.5f;
    }

    // Bitonicを昇順で実行しながら、-depthをkeyにして最終描画順を奥→手前にする。
    return -(clipPosition.z / clipPosition.w);
}

bool IsGreater(uint leftParticleIndex, uint rightParticleIndex)
{
    float leftKey = ResolveSortKey(leftParticleIndex);
    float rightKey = ResolveSortKey(rightParticleIndex);
    if (leftKey != rightKey)
    {
        return leftKey > rightKey;
    }
    return leftParticleIndex > rightParticleIndex;
}

bool IsLess(uint leftParticleIndex, uint rightParticleIndex)
{
    float leftKey = ResolveSortKey(leftParticleIndex);
    float rightKey = ResolveSortKey(rightParticleIndex);
    if (leftKey != rightKey)
    {
        return leftKey < rightKey;
    }
    return leftParticleIndex < rightParticleIndex;
}

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint sortCapacity = ResolveSortCapacity();
    uint index = dispatchThreadId.x;
    if (gSort.sortLevel > sortCapacity || index >= sortCapacity)
    {
        return;
    }

    uint partnerIndex = index ^ gSort.sortLevelMask;
    if (partnerIndex <= index || partnerIndex >= sortCapacity)
    {
        return;
    }

    uint leftParticleIndex = gVisibleParticleIndices[index];
    uint rightParticleIndex = gVisibleParticleIndices[partnerIndex];
    bool ascending = (index & gSort.sortLevel) == 0u;
    bool shouldSwap = ascending
        ? IsGreater(leftParticleIndex, rightParticleIndex)
        : IsLess(leftParticleIndex, rightParticleIndex);

    if (shouldSwap)
    {
        gVisibleParticleIndices[index] = rightParticleIndex;
        gVisibleParticleIndices[partnerIndex] = leftParticleIndex;
    }
}
