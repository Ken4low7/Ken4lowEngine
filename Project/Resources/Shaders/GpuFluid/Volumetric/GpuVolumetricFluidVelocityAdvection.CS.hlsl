#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
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
    const float3 uvw = GpuVolumetricFluidCellToUvw(cell, gFluid);
    const float3 currentVelocity =
        gVelocityRead.SampleLevel(gLinearClampSampler, uvw, 0.0f).xyz;

    const float3 inverseGridSize = float3(
        gFluid.invGridWidth,
        gFluid.invGridHeight,
        gFluid.invGridDepth);
    const float3 backtraceUvwOffset =
        currentVelocity * gFluid.deltaTime * gFluid.invCellSize * inverseGridSize;
    const float3 sourceUvw = GpuVolumetricFluidClampUvwToCellCenters(
        uvw - backtraceUvwOffset,
        gFluid);

    // Texture3D + Linear Samplerで8近傍voxelを補間し、3D速度場をsemi-Lagrangian移流する。
    const float3 advectedVelocity =
        gVelocityRead.SampleLevel(gLinearClampSampler, sourceUvw, 0.0f).xyz;
    gVelocityWrite[cell] = float4(
        advectedVelocity * gFluid.velocityDissipation,
        0.0f);
}
