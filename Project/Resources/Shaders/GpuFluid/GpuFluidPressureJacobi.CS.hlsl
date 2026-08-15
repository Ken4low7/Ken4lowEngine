#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float> gDivergence : register(t0);
Texture2D<float> gPressureRead : register(t1);
RWTexture2D<float> gPressureWrite : register(u0);

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

	const float pressureLeft = gPressureRead.Load(int3(leftCell, 0));
	const float pressureRight = gPressureRead.Load(int3(rightCell, 0));
	const float pressureBottom = gPressureRead.Load(int3(bottomCell, 0));
	const float pressureTop = gPressureRead.Load(int3(topCell, 0));
	const float divergence = gDivergence.Load(int3(cell, 0));
	const float cellSizeSquared = gFluid.cellSize * gFluid.cellSize;

	// ∇²p=div(u)をJacobi反復で解き、Projectionで使う速度補正用スカラー場を得る。
	gPressureWrite[dispatchThreadId.xy] =
		(pressureLeft + pressureRight + pressureBottom + pressureTop -
			divergence * cellSizeSquared) * 0.25f;
}
