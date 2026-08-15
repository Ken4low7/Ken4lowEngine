#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocity : register(t0);
RWTexture2D<float> gDivergence : register(u0);

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

	const float2 velocityLeft = gVelocity.Load(int3(leftCell, 0));
	const float2 velocityRight = gVelocity.Load(int3(rightCell, 0));
	const float2 velocityBottom = gVelocity.Load(int3(bottomCell, 0));
	const float2 velocityTop = gVelocity.Load(int3(topCell, 0));

	// 中心差分で速度場の発散を測り、Pressure solveが除去すべき圧縮成分を作る。
	const float divergence = 0.5f * gFluid.invCellSize *
		((velocityRight.x - velocityLeft.x) + (velocityTop.y - velocityBottom.y));
	gDivergence[dispatchThreadId.xy] = divergence;
}
