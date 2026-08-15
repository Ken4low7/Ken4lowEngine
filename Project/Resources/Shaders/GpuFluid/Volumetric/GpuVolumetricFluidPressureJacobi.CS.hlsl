#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float> gDivergence : register(t0);
Texture3D<float> gPressureRead : register(t1);
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
    const float centerPressure = gPressureRead.Load(int4(cell, 0));

    float pressureLeft = centerPressure;
    float pressureRight = centerPressure;
    float pressureBottom = centerPressure;
    float pressureTop = centerPressure;
    float pressureBack = centerPressure;
    float pressureFront = centerPressure;

    if (cell.x > 0)
    {
        pressureLeft = gPressureRead.Load(int4(cell + int3(-1, 0, 0), 0));
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        pressureRight = gPressureRead.Load(int4(cell + int3(1, 0, 0), 0));
    }
    if (cell.y > 0)
    {
        pressureBottom = gPressureRead.Load(int4(cell + int3(0, -1, 0), 0));
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        pressureTop = gPressureRead.Load(int4(cell + int3(0, 1, 0), 0));
    }
    if (cell.z > 0)
    {
        pressureBack = gPressureRead.Load(int4(cell + int3(0, 0, -1), 0));
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        pressureFront = gPressureRead.Load(int4(cell + int3(0, 0, 1), 0));
    }

    const float divergence = gDivergence.Load(int4(cell, 0));
    const float cellSizeSquared = gFluid.cellSize * gFluid.cellSize;

    // 3D Poissonを6近傍Jacobiで解き、外周は中心Pressureを使うNeumann境界にする。
    gPressureWrite[dispatchThreadId] =
        (pressureLeft + pressureRight +
         pressureBottom + pressureTop +
         pressureBack + pressureFront -
         divergence * cellSizeSquared) / 6.0f;
}
