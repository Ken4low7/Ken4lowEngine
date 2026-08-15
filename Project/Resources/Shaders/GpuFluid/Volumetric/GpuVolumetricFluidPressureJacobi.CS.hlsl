#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float> gDivergence : register(t0);
Texture3D<float> gPressureRead : register(t1);
Texture3D<uint> gObstacle : register(t2);
RWTexture3D<float> gPressureWrite : register(u0);

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
        gPressureWrite[dispatchThreadId] = 0.0f;
        return;
    }

    const float centerPressure = gPressureRead.Load(int4(cell, 0));
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

    if (cell.x > 0)
    {
        if (gObstacle.Load(int4(leftCell, 0)) == 0u)
        {
            pressureLeft = gPressureRead.Load(int4(leftCell, 0));
        }
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        if (gObstacle.Load(int4(rightCell, 0)) == 0u)
        {
            pressureRight = gPressureRead.Load(int4(rightCell, 0));
        }
    }
    if (cell.y > 0)
    {
        if (gObstacle.Load(int4(bottomCell, 0)) == 0u)
        {
            pressureBottom = gPressureRead.Load(int4(bottomCell, 0));
        }
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        if (gObstacle.Load(int4(topCell, 0)) == 0u)
        {
            pressureTop = gPressureRead.Load(int4(topCell, 0));
        }
    }
    if (cell.z > 0)
    {
        if (gObstacle.Load(int4(backCell, 0)) == 0u)
        {
            pressureBack = gPressureRead.Load(int4(backCell, 0));
        }
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        if (gObstacle.Load(int4(frontCell, 0)) == 0u)
        {
            pressureFront = gPressureRead.Load(int4(frontCell, 0));
        }
    }

    const float divergence = gDivergence.Load(int4(cell, 0));
    const float cellSizeSquared = gFluid.cellSize * gFluid.cellSize;

    // Solid neighborとVolume外側はcenterPressureを使うNeumann境界としてPressure勾配を壁外へ作らない。
    gPressureWrite[dispatchThreadId] =
        (pressureLeft + pressureRight +
         pressureBottom + pressureTop +
         pressureBack + pressureFront -
         divergence * cellSizeSquared) / 6.0f;
}
