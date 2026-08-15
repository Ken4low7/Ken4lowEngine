#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<uint> gObstacle : register(t3);
RWTexture2D<float> gVorticityWrite : register(u0);

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
		gVorticityWrite[dispatchThreadId.xy] = 0.0f;
		return;
	}

	const int2 leftCell = GpuFluidClampCell(cell + int2(-1, 0), gFluid);
	const int2 rightCell = GpuFluidClampCell(cell + int2(1, 0), gFluid);
	const int2 bottomCell = GpuFluidClampCell(cell + int2(0, -1), gFluid);
	const int2 topCell = GpuFluidClampCell(cell + int2(0, 1), gFluid);
	const float2 zeroVelocity = float2(0.0f, 0.0f);
	const float2 velocityLeft = gObstacle.Load(int3(leftCell, 0)) != 0u ? zeroVelocity : gVelocityRead.Load(int3(leftCell, 0));
	const float2 velocityRight = gObstacle.Load(int3(rightCell, 0)) != 0u ? zeroVelocity : gVelocityRead.Load(int3(rightCell, 0));
	const float2 velocityBottom = gObstacle.Load(int3(bottomCell, 0)) != 0u ? zeroVelocity : gVelocityRead.Load(int3(bottomCell, 0));
	const float2 velocityTop = gObstacle.Load(int3(topCell, 0)) != 0u ? zeroVelocity : gVelocityRead.Load(int3(topCell, 0));

	const float curl =
		((velocityRight.y - velocityLeft.y) -
		 (velocityTop.x - velocityBottom.x)) *
		(0.5f * gFluid.invCellSize);
	gVorticityWrite[dispatchThreadId.xy] = curl;
}
