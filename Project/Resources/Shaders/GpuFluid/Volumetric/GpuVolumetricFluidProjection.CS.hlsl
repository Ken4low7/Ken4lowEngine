#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float> gPressure : register(t1);
Texture3D<uint> gObstacle : register(t2);
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
    if (gObstacle.Load(int4(cell, 0)) != 0u)
    {
        gVelocityWrite[dispatchThreadId] = 0.0f;
        return;
    }

    const float centerPressure = gPressure.Load(int4(cell, 0));
    float pressureLeft = centerPressure;
    float pressureRight = centerPressure;
    float pressureBottom = centerPressure;
    float pressureTop = centerPressure;
    float pressureBack = centerPressure;
    float pressureFront = centerPressure;

    const int3 leftCell = cell + int3(-1, 0, 0);
    const int3 rightCell = cell + int3(1, 0, 0);
    const int3 bottomCell = cell + int3(0, -1, 0);
    const int3 topCell = cell + int3(0, 1, 0);
    const int3 backCell = cell + int3(0, 0, -1);
    const int3 frontCell = cell + int3(0, 0, 1);

    bool blockedLeft = cell.x <= 0;
    bool blockedRight = cell.x + 1 >= int(gFluid.gridWidth);
    bool blockedBottom = cell.y <= 0;
    bool blockedTop = cell.y + 1 >= int(gFluid.gridHeight);
    bool blockedBack = cell.z <= 0;
    bool blockedFront = cell.z + 1 >= int(gFluid.gridDepth);

    if (!blockedLeft)
    {
        blockedLeft = gObstacle.Load(int4(leftCell, 0)) != 0u;
        if (!blockedLeft) pressureLeft = gPressure.Load(int4(leftCell, 0));
    }
    if (!blockedRight)
    {
        blockedRight = gObstacle.Load(int4(rightCell, 0)) != 0u;
        if (!blockedRight) pressureRight = gPressure.Load(int4(rightCell, 0));
    }
    if (!blockedBottom)
    {
        blockedBottom = gObstacle.Load(int4(bottomCell, 0)) != 0u;
        if (!blockedBottom) pressureBottom = gPressure.Load(int4(bottomCell, 0));
    }
    if (!blockedTop)
    {
        blockedTop = gObstacle.Load(int4(topCell, 0)) != 0u;
        if (!blockedTop) pressureTop = gPressure.Load(int4(topCell, 0));
    }
    if (!blockedBack)
    {
        blockedBack = gObstacle.Load(int4(backCell, 0)) != 0u;
        if (!blockedBack) pressureBack = gPressure.Load(int4(backCell, 0));
    }
    if (!blockedFront)
    {
        blockedFront = gObstacle.Load(int4(frontCell, 0)) != 0u;
        if (!blockedFront) pressureFront = gPressure.Load(int4(frontCell, 0));
    }

    const float3 pressureGradient = 0.5f * gFluid.invCellSize * float3(
        pressureRight - pressureLeft,
        pressureTop - pressureBottom,
        pressureFront - pressureBack);

    float3 projectedVelocity =
        gVelocityRead.Load(int4(cell, 0)).xyz - pressureGradient;

    // Solid隣接面とDomain外周では該当軸の法線速度だけ0にし、接線方向の壁沿い流れは残す。
    if (blockedLeft || blockedRight)
    {
        projectedVelocity.x = 0.0f;
    }
    if (blockedBottom || blockedTop)
    {
        projectedVelocity.y = 0.0f;
    }
    if (blockedBack || blockedFront)
    {
        projectedVelocity.z = 0.0f;
    }

    gVelocityWrite[dispatchThreadId] = float4(projectedVelocity, 0.0f);
}
