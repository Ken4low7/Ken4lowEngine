#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
RWTexture2D<float> gVorticityWrite : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const int2 cell = int2(dispatchThreadId.xy);
	const float2 velocityLeft = gVelocityRead.Load(int3(GpuFluidClampCell(cell + int2(-1, 0), gFluid), 0));
	const float2 velocityRight = gVelocityRead.Load(int3(GpuFluidClampCell(cell + int2(1, 0), gFluid), 0));
	const float2 velocityBottom = gVelocityRead.Load(int3(GpuFluidClampCell(cell + int2(0, -1), gFluid), 0));
	const float2 velocityTop = gVelocityRead.Load(int3(GpuFluidClampCell(cell + int2(0, 1), gFluid), 0));

	// 2D Curlのz成分だけを保持し、Confinement方向計算の中間場として使う。
	const float curl =
		((velocityRight.y - velocityLeft.y) -
		 (velocityTop.x - velocityBottom.x)) *
		(0.5f * gFluid.invCellSize);
	gVorticityWrite[dispatchThreadId.xy] = curl;
}
