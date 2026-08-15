#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float> gPressure : register(t1);
RWTexture3D<float4> gVelocityWrite : register(u0);

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFluid.gridWidth ||
        dispatchThreadId.y >= gFluid.gridHeight ||
        dispatchThreadId.z >= gFluid.gridDepth)
    {
        return;
    }

    const int3 cell = int3(dispatchThreadId);
    const float centerPressure = gPressure.Load(int4(cell, 0));

    float pressureLeft = centerPressure;
    float pressureRight = centerPressure;
    float pressureBottom = centerPressure;
    float pressureTop = centerPressure;
    float pressureBack = centerPressure;
    float pressureFront = centerPressure;

    if (cell.x > 0)
    {
        pressureLeft = gPressure.Load(int4(cell + int3(-1, 0, 0), 0));
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        pressureRight = gPressure.Load(int4(cell + int3(1, 0, 0), 0));
    }
    if (cell.y > 0)
    {
        pressureBottom = gPressure.Load(int4(cell + int3(0, -1, 0), 0));
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        pressureTop = gPressure.Load(int4(cell + int3(0, 1, 0), 0));
    }
    if (cell.z > 0)
    {
        pressureBack = gPressure.Load(int4(cell + int3(0, 0, -1), 0));
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        pressureFront = gPressure.Load(int4(cell + int3(0, 0, 1), 0));
    }

    const float3 pressureGradient = 0.5f * gFluid.invCellSize * float3(
        pressureRight - pressureLeft,
        pressureTop - pressureBottom,
        pressureFront - pressureBack);

    float3 projectedVelocity =
        gVelocityRead.Load(int4(cell, 0)).xyz - pressureGradient;

    // 閉じたVolumeの6面では法線速度を0にし、外へ流体が抜けない境界条件を保証する。
    if (dispatchThreadId.x == 0 || dispatchThreadId.x + 1 >= gFluid.gridWidth)
    {
        projectedVelocity.x = 0.0f;
    }
    if (dispatchThreadId.y == 0 || dispatchThreadId.y + 1 >= gFluid.gridHeight)
    {
        projectedVelocity.y = 0.0f;
    }
    if (dispatchThreadId.z == 0 || dispatchThreadId.z + 1 >= gFluid.gridDepth)
    {
        projectedVelocity.z = 0.0f;
    }

    gVelocityWrite[dispatchThreadId] = float4(projectedVelocity, 0.0f);
}
