#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocity : register(t0);
Texture3D<uint> gObstacle : register(t2);
RWTexture3D<float> gDivergence : register(u0);

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
        gDivergence[dispatchThreadId] = 0.0f;
        return;
    }

    float3 velocityLeft = 0.0f;
    float3 velocityRight = 0.0f;
    float3 velocityBottom = 0.0f;
    float3 velocityTop = 0.0f;
    float3 velocityBack = 0.0f;
    float3 velocityFront = 0.0f;

    const int3 leftCell = cell + int3(-1, 0, 0);
    const int3 rightCell = cell + int3(1, 0, 0);
    const int3 bottomCell = cell + int3(0, -1, 0);
    const int3 topCell = cell + int3(0, 1, 0);
    const int3 backCell = cell + int3(0, 0, -1);
    const int3 frontCell = cell + int3(0, 0, 1);

    if (cell.x > 0)
    {
        if (gObstacle.Load(int4(leftCell, 0)) == 0u)
        {
            velocityLeft = gVelocity.Load(int4(leftCell, 0)).xyz;
        }
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        if (gObstacle.Load(int4(rightCell, 0)) == 0u)
        {
            velocityRight = gVelocity.Load(int4(rightCell, 0)).xyz;
        }
    }
    if (cell.y > 0)
    {
        if (gObstacle.Load(int4(bottomCell, 0)) == 0u)
        {
            velocityBottom = gVelocity.Load(int4(bottomCell, 0)).xyz;
        }
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        if (gObstacle.Load(int4(topCell, 0)) == 0u)
        {
            velocityTop = gVelocity.Load(int4(topCell, 0)).xyz;
        }
    }
    if (cell.z > 0)
    {
        if (gObstacle.Load(int4(backCell, 0)) == 0u)
        {
            velocityBack = gVelocity.Load(int4(backCell, 0)).xyz;
        }
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        if (gObstacle.Load(int4(frontCell, 0)) == 0u)
        {
            velocityFront = gVelocity.Load(int4(frontCell, 0)).xyz;
        }
    }

    // Domain外とSolid neighborを速度0として扱い、壁を横切るFluxをDivergenceへ入れない。
    const float divergence = 0.5f * gFluid.invCellSize *
        ((velocityRight.x - velocityLeft.x) +
         (velocityTop.y - velocityBottom.y) +
         (velocityFront.z - velocityBack.z));
    gDivergence[dispatchThreadId] = divergence;
}
