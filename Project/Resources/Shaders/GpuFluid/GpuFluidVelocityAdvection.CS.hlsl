#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
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
	const float2 uv = GpuFluidCellToUv(cell, gFluid);
	const float2 currentVelocity = gVelocityRead.SampleLevel(gLinearClampSampler, uv, 0.0f);

	// 速度をworld units/secとして扱い、逆追跡距離をcell数から正規化UVへ変換する。
	const float2 backtraceUvOffset =
		currentVelocity * gFluid.deltaTime * gFluid.invCellSize *
		float2(gFluid.invGridWidth, gFluid.invGridHeight);
	const float2 sourceUv = GpuFluidClampUvToCellCenters(uv - backtraceUvOffset, gFluid);
	const float2 advectedVelocity =
		gVelocityRead.SampleLevel(gLinearClampSampler, sourceUv, 0.0f);

	gVelocityWrite[cell] = advectedVelocity * gFluid.velocityDissipation;
}
