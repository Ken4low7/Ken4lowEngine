#include "GpuVolumetricFluidCommon.hlsli"

struct GpuVolumetricFluidObstacleGpuData
{
    uint shapeType;
    uint3 paddingType;

    float3 worldCenter;
    float radius;

    float3 halfSize;
    float paddingHalfSize;

    float3 axisX;
    float paddingAxisX;

    float3 axisY;
    float paddingAxisY;

    float3 axisZ;
    float paddingAxisZ;
};

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

cbuffer FluidObstacleRasterCB : register(b1)
{
    float3 gDomainOrigin;
    uint gObstacleCount;

    float3 gDomainAxisU;
    float gDomainCellSize;

    float3 gDomainAxisV;
    float gObstacleRasterPadding0;

    float3 gDomainAxisW;
    float gObstacleRasterPadding1;
};

StructuredBuffer<GpuVolumetricFluidObstacleGpuData> gObstacles : register(t0);
RWTexture3D<uint> gObstacleMask : register(u0);

bool IsInsideObstacle(float3 worldPosition, GpuVolumetricFluidObstacleGpuData obstacle)
{
    const float3 offset = worldPosition - obstacle.worldCenter;
    if (obstacle.shapeType == 0u)
    {
        return dot(offset, offset) <= obstacle.radius * obstacle.radius;
    }

    const float3 localDistance = float3(
        abs(dot(offset, obstacle.axisX)),
        abs(dot(offset, obstacle.axisY)),
        abs(dot(offset, obstacle.axisZ)));
    return all(localDistance <= obstacle.halfSize);
}

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFluid.gridWidth ||
        dispatchThreadId.y >= gFluid.gridHeight ||
        dispatchThreadId.z >= gFluid.gridDepth)
    {
        return;
    }

    const float3 cellCenter = float3(dispatchThreadId) + 0.5f;
    const float3 worldPosition =
        gDomainOrigin +
        gDomainAxisU * (cellCenter.x * gDomainCellSize) +
        gDomainAxisV * (cellCenter.y * gDomainCellSize) +
        gDomainAxisW * (cellCenter.z * gDomainCellSize);

    uint solid = 0u;
    for (uint obstacleIndex = 0; obstacleIndex < gObstacleCount; ++obstacleIndex)
    {
        if (IsInsideObstacle(worldPosition, gObstacles[obstacleIndex]))
        {
            solid = 1u;
            break;
        }
    }

    // 3D Maskを毎Frame全voxel上書きし、移動/削除Colliderの残像を残さない。
    gObstacleMask[dispatchThreadId] = solid;
}
