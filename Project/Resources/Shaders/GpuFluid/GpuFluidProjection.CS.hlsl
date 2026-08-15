#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gPressure : register(t1);
Texture2D<uint> gObstacle : register(t2);
RWTexture2D<float2> gVelocityWrite : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const int2 cell = int2(dispatchThreadId.xy);
	if (gObstacle.Load(int3(cell, 0)) != 0u)
	{
		gVelocityWrite[dispatchThreadId.xy] = 0.0f;
		return;
	}

	const int2 leftCell = GpuFluidClampCell(cell + int2(-1, 0), gFluid);
	const int2 rightCell = GpuFluidClampCell(cell + int2(1, 0), gFluid);
	const int2 bottomCell = GpuFluidClampCell(cell + int2(0, -1), gFluid);
	const int2 topCell = GpuFluidClampCell(cell + int2(0, 1), gFluid);
	const float centerPressure = gPressure.Load(int3(cell, 0));

	const bool solidLeft = gObstacle.Load(int3(leftCell, 0)) != 0u;
	const bool solidRight = gObstacle.Load(int3(rightCell, 0)) != 0u;
	const bool solidBottom = gObstacle.Load(int3(bottomCell, 0)) != 0u;
	const bool solidTop = gObstacle.Load(int3(topCell, 0)) != 0u;
	const float pressureLeft = solidLeft ? centerPressure : gPressure.Load(int3(leftCell, 0));
	const float pressureRight = solidRight ? centerPressure : gPressure.Load(int3(rightCell, 0));
	const float pressureBottom = solidBottom ? centerPressure : gPressure.Load(int3(bottomCell, 0));
	const float pressureTop = solidTop ? centerPressure : gPressure.Load(int3(topCell, 0));
	const float2 pressureGradient = 0.5f * gFluid.invCellSize *
		float2(pressureRight - pressureLeft, pressureTop - pressureBottom);

	float2 projectedVelocity = gVelocityRead.Load(int3(cell, 0)) - pressureGradient;

	// Solid隣接面とSimulation外周の法線速度を0にして、Colliderを横切る流れを止める。
	if (solidLeft || solidRight || dispatchThreadId.x == 0 || dispatchThreadId.x + 1 >= gFluid.gridWidth)
	{
		projectedVelocity.x = 0.0f;
	}
	if (solidBottom || solidTop || dispatchThreadId.y == 0 || dispatchThreadId.y + 1 >= gFluid.gridHeight)
	{
		projectedVelocity.y = 0.0f;
	}

	gVelocityWrite[dispatchThreadId.xy] = projectedVelocity;
}
