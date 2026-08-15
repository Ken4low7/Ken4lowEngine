#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float> gDivergence : register(t0);
Texture2D<float> gPressureRead : register(t1);
Texture2D<uint> gObstacle : register(t2);
RWTexture2D<float> gPressureWrite : register(u0);

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
		gPressureWrite[dispatchThreadId.xy] = 0.0f;
		return;
	}

	const int2 leftCell = GpuFluidClampCell(cell + int2(-1, 0), gFluid);
	const int2 rightCell = GpuFluidClampCell(cell + int2(1, 0), gFluid);
	const int2 bottomCell = GpuFluidClampCell(cell + int2(0, -1), gFluid);
	const int2 topCell = GpuFluidClampCell(cell + int2(0, 1), gFluid);
	const float centerPressure = gPressureRead.Load(int3(cell, 0));

	// Solid neighborは中心Pressureを使うNeumann境界として扱い、壁内部へPressure勾配を作らない。
	const float pressureLeft = gObstacle.Load(int3(leftCell, 0)) != 0u ? centerPressure : gPressureRead.Load(int3(leftCell, 0));
	const float pressureRight = gObstacle.Load(int3(rightCell, 0)) != 0u ? centerPressure : gPressureRead.Load(int3(rightCell, 0));
	const float pressureBottom = gObstacle.Load(int3(bottomCell, 0)) != 0u ? centerPressure : gPressureRead.Load(int3(bottomCell, 0));
	const float pressureTop = gObstacle.Load(int3(topCell, 0)) != 0u ? centerPressure : gPressureRead.Load(int3(topCell, 0));
	const float divergence = gDivergence.Load(int3(cell, 0));
	const float cellSizeSquared = gFluid.cellSize * gFluid.cellSize;

	gPressureWrite[dispatchThreadId.xy] =
		(pressureLeft + pressureRight + pressureBottom + pressureTop -
			divergence * cellSizeSquared) * 0.25f;
}
