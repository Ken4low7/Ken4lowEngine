#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gPressure : register(t1);
RWTexture2D<float2> gVelocityWrite : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const int2 cell = int2(dispatchThreadId.xy);
	const int2 leftCell = GpuFluidClampCell(cell + int2(-1, 0), gFluid);
	const int2 rightCell = GpuFluidClampCell(cell + int2(1, 0), gFluid);
	const int2 bottomCell = GpuFluidClampCell(cell + int2(0, -1), gFluid);
	const int2 topCell = GpuFluidClampCell(cell + int2(0, 1), gFluid);

	const float pressureLeft = gPressure.Load(int3(leftCell, 0));
	const float pressureRight = gPressure.Load(int3(rightCell, 0));
	const float pressureBottom = gPressure.Load(int3(bottomCell, 0));
	const float pressureTop = gPressure.Load(int3(topCell, 0));
	const float2 pressureGradient = 0.5f * gFluid.invCellSize *
		float2(pressureRight - pressureLeft, pressureTop - pressureBottom);

	float2 projectedVelocity = gVelocityRead.Load(int3(cell, 0)) - pressureGradient;

	// 16.8のObstacle境界へ拡張しやすいよう、まずSimulation領域外周だけ法線速度0を保証する。
	if (dispatchThreadId.x == 0 || dispatchThreadId.x + 1 >= gFluid.gridWidth)
	{
		projectedVelocity.x = 0.0f;
	}
	if (dispatchThreadId.y == 0 || dispatchThreadId.y + 1 >= gFluid.gridHeight)
	{
		projectedVelocity.y = 0.0f;
	}

	gVelocityWrite[dispatchThreadId.xy] = projectedVelocity;
}
