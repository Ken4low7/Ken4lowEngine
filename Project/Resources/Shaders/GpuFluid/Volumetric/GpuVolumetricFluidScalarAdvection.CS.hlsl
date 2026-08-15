#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

cbuffer ScalarAdvectionCB : register(b1)
{
    float gScalarDissipation;
    float3 gScalarAdvectionPadding;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float> gScalarRead : register(t1);
Texture3D<uint> gObstacle : register(t2);
RWTexture3D<float> gScalarWrite : register(u0);
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
        gScalarWrite[cell] = 0.0f;
        return;
    }

    const float3 uvw = GpuVolumetricFluidCellToUvw(cell, gFluid);
    const float3 velocity =
        gVelocityRead.SampleLevel(gLinearClampSampler, uvw, 0.0f).xyz;
    const float3 inverseGridSize = float3(
        gFluid.invGridWidth,
        gFluid.invGridHeight,
        gFluid.invGridDepth);
    const float3 backtraceUvwOffset =
        velocity * gFluid.deltaTime * gFluid.invCellSize * inverseGridSize;
    float3 sourceUvw = GpuVolumetricFluidClampUvwToCellCenters(
        uvw - backtraceUvwOffset,
        gFluid);

    const uint3 sourceCell = min(
        uint3(sourceUvw * float3(gFluid.gridWidth, gFluid.gridHeight, gFluid.gridDepth)),
        uint3(gFluid.gridWidth - 1u, gFluid.gridHeight - 1u, gFluid.gridDepth - 1u));
    if (gObstacle.Load(int4(sourceCell, 0)) != 0u)
    {
        sourceUvw = uvw; // Density/TemperatureをSolidの向こう側から直接引き込まない。
    }

    const float advectedScalar =
        gScalarRead.SampleLevel(gLinearClampSampler, sourceUvw, 0.0f);
    gScalarWrite[cell] = advectedScalar * gScalarDissipation;
}
