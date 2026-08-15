#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float> gDensityRead : register(t1);
Texture3D<float> gTemperatureRead : register(t2);
Texture3D<uint> gObstacle : register(t3);
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

    const float density = gDensityRead.Load(int4(cell, 0));
    const float temperature = gTemperatureRead.Load(int4(cell, 0));
    float3 velocity = gVelocityRead.Load(int4(cell, 0)).xyz;

    const float buoyancyForce =
        gFluid.buoyancy * (temperature - gFluid.ambientTemperature) -
        gFluid.smokeWeight * density;
    velocity.y += buoyancyForce * gFluid.deltaTime;

    bool blockedLeft = cell.x <= 0;
    bool blockedRight = cell.x + 1 >= int(gFluid.gridWidth);
    bool blockedBottom = cell.y <= 0;
    bool blockedTop = cell.y + 1 >= int(gFluid.gridHeight);
    bool blockedBack = cell.z <= 0;
    bool blockedFront = cell.z + 1 >= int(gFluid.gridDepth);

    if (!blockedLeft) blockedLeft = gObstacle.Load(int4(cell + int3(-1, 0, 0), 0)) != 0u;
    if (!blockedRight) blockedRight = gObstacle.Load(int4(cell + int3(1, 0, 0), 0)) != 0u;
    if (!blockedBottom) blockedBottom = gObstacle.Load(int4(cell + int3(0, -1, 0), 0)) != 0u;
    if (!blockedTop) blockedTop = gObstacle.Load(int4(cell + int3(0, 1, 0), 0)) != 0u;
    if (!blockedBack) blockedBack = gObstacle.Load(int4(cell + int3(0, 0, -1), 0)) != 0u;
    if (!blockedFront) blockedFront = gObstacle.Load(int4(cell + int3(0, 0, 1), 0)) != 0u;

    // Buoyancy適用中もSolid/Domain境界を横切る法線速度だけを0へ戻す。
    if (blockedLeft || blockedRight)
    {
        velocity.x = 0.0f;
    }
    if (blockedBottom || blockedTop)
    {
        velocity.y = 0.0f;
    }
    if (blockedBack || blockedFront)
    {
        velocity.z = 0.0f;
    }

    gVelocityWrite[dispatchThreadId] = float4(velocity, 0.0f);
}
