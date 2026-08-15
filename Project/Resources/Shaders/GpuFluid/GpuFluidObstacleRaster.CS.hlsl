#include "GpuFluidCommon.hlsli"

struct GpuFluidObstacleGpuData
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
	GpuFluidSimulationConstants gFluid;
};

cbuffer FluidObstacleRasterCB : register(b1)
{
	float3 gDomainOrigin;
	uint gObstacleCount;

	float3 gDomainAxisU;
	float gDomainCellSize;

	float3 gDomainAxisV;
	float gObstacleRasterPadding;
};

StructuredBuffer<GpuFluidObstacleGpuData> gObstacles : register(t0);
RWTexture2D<uint> gObstacleMask : register(u0);

bool IsInsideObstacle(float3 worldPosition, GpuFluidObstacleGpuData obstacle)
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

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const float2 cellCenter = float2(dispatchThreadId.xy) + 0.5f;
	const float3 worldPosition =
		gDomainOrigin +
		gDomainAxisU * (cellCenter.x * gDomainCellSize) +
		gDomainAxisV * (cellCenter.y * gDomainCellSize);

	uint solid = 0u;
	for (uint obstacleIndex = 0; obstacleIndex < gObstacleCount; ++obstacleIndex)
	{
		if (IsInsideObstacle(worldPosition, gObstacles[obstacleIndex]))
		{
			solid = 1u;
			break;
		}
	}

	// Maskは毎Dispatch全セルを書き直し、移動Colliderや削除Colliderの残像を残さない。
	gObstacleMask[dispatchThreadId.xy] = solid;
}
