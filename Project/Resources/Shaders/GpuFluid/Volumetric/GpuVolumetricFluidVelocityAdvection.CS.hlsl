#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<uint> gObstacle : register(t1);
RWTexture3D<float4> gVelocityWrite : register(u0);
SamplerState gLinearClampSampler : register(s0);

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFluid.gridWidth ||
        dispatchThreadId.y >= gFluid.gridHeight ||
        dispatchThreadId.z >= gFluid.gridDepth)
    {
        return;
    }

    const uint3 cell = dispatchThreadId;
    if (gObstacle.Load(int4(cell, 0)) != 0u)
    {
        gVelocityWrite[cell] = 0.0f;
        return;
    }

    const float3 uvw = GpuVolumetricFluidCellToUvw(cell, gFluid);
    const float3 currentVelocity =
        gVelocityRead.SampleLevel(gLinearClampSampler, uvw, 0.0f).xyz;

    const float3 inverseGridSize = float3(
        gFluid.invGridWidth,
        gFluid.invGridHeight,
        gFluid.invGridDepth);
    const float3 backtraceUvwOffset =
        currentVelocity * gFluid.deltaTime * gFluid.invCellSize * inverseGridSize;
    float3 sourceUvw = GpuVolumetricFluidClampUvwToCellCenters(
        uvw - backtraceUvwOffset,
        gFluid);

    const uint3 sourceCell = min(
        uint3(sourceUvw * float3(gFluid.gridWidth, gFluid.gridHeight, gFluid.gridDepth)),
        uint3(gFluid.gridWidth - 1u, gFluid.gridHeight - 1u, gFluid.gridDepth - 1u));
    if (gObstacle.Load(int4(sourceCell, 0)) != 0u)
    {
        sourceUvw = uvw; // Solidの向こう側から速度を直接引き込まず、壁に当たった逆追跡を現在voxelで止める。
    }

    const float3 advectedVelocity =
        gVelocityRead.SampleLevel(gLinearClampSampler, sourceUvw, 0.0f).xyz;
    gVelocityWrite[cell] = float4(
        advectedVelocity * gFluid.velocityDissipation,
        0.0f);
}
