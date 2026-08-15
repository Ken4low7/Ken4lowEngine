#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
RWTexture3D<float4> gVorticityWrite : register(u0);

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
    float3 velocityLeft = 0.0f;
    float3 velocityRight = 0.0f;
    float3 velocityBottom = 0.0f;
    float3 velocityTop = 0.0f;
    float3 velocityBack = 0.0f;
    float3 velocityFront = 0.0f;

    if (cell.x > 0)
    {
        velocityLeft = gVelocityRead.Load(int4(cell + int3(-1, 0, 0), 0)).xyz;
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        velocityRight = gVelocityRead.Load(int4(cell + int3(1, 0, 0), 0)).xyz;
    }
    if (cell.y > 0)
    {
        velocityBottom = gVelocityRead.Load(int4(cell + int3(0, -1, 0), 0)).xyz;
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        velocityTop = gVelocityRead.Load(int4(cell + int3(0, 1, 0), 0)).xyz;
    }
    if (cell.z > 0)
    {
        velocityBack = gVelocityRead.Load(int4(cell + int3(0, 0, -1), 0)).xyz;
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        velocityFront = gVelocityRead.Load(int4(cell + int3(0, 0, 1), 0)).xyz;
    }

    const float halfInverseCell = 0.5f * gFluid.invCellSize;
    const float3 curl = float3(
        (velocityTop.z - velocityBottom.z) - (velocityFront.y - velocityBack.y),
        (velocityFront.x - velocityBack.x) - (velocityRight.z - velocityLeft.z),
        (velocityRight.y - velocityLeft.y) - (velocityTop.x - velocityBottom.x)) * halfInverseCell;

    // 3DではCurlをxyzベクトルとして保持し、Confinementで回転軸と向きをそのまま利用する。
    gVorticityWrite[dispatchThreadId] = float4(curl, 0.0f);
}
