#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<uint> gObstacle : register(t1);
RWTexture2D<float2> gVelocityWrite : register(u0);
SamplerState gLinearClampSampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const uint2 cell = dispatchThreadId.xy;
	if (gObstacle.Load(int3(cell, 0)) != 0u)
	{
		gVelocityWrite[cell] = 0.0f;
		return;
	}

	const float2 uv = GpuFluidCellToUv(cell, gFluid);
	const float2 currentVelocity = gVelocityRead.SampleLevel(gLinearClampSampler, uv, 0.0f);

	const float2 backtraceUvOffset =
		currentVelocity * gFluid.deltaTime * gFluid.invCellSize *
		float2(gFluid.invGridWidth, gFluid.invGridHeight);
	float2 sourceUv = GpuFluidClampUvToCellCenters(uv - backtraceUvOffset, gFluid);
	const uint2 sourceCell = min(
		uint2(sourceUv * float2(gFluid.gridWidth, gFluid.gridHeight)),
		uint2(gFluid.gridWidth - 1u, gFluid.gridHeight - 1u));
	if (gObstacle.Load(int3(sourceCell, 0)) != 0u)
	{
		sourceUv = uv; // 逆追跡先がSolidなら壁の向こう側をSampleせず、現在のFluid Cellへ留める。
	}

	const float2 advectedVelocity =
		gVelocityRead.SampleLevel(gLinearClampSampler, sourceUv, 0.0f);
	gVelocityWrite[cell] = advectedVelocity * gFluid.velocityDissipation;
}
