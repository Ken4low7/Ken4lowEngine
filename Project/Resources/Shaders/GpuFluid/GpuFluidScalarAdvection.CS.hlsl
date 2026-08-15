#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

cbuffer ScalarAdvectionCB : register(b1)
{
	float gScalarDissipation;
	float3 gScalarAdvectionPadding;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gScalarRead : register(t1);
RWTexture2D<float> gScalarWrite : register(u0);
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
	const float2 velocity = gVelocityRead.SampleLevel(gLinearClampSampler, uv, 0.0f);

	// Density/Temperatureの両方で同じ半ラグランジュ逆追跡規約を共有する。
	const float2 backtraceUvOffset =
		velocity * gFluid.deltaTime * gFluid.invCellSize *
		float2(gFluid.invGridWidth, gFluid.invGridHeight);
	const float2 sourceUv = GpuFluidClampUvToCellCenters(uv - backtraceUvOffset, gFluid);
	const float advectedScalar = gScalarRead.SampleLevel(gLinearClampSampler, sourceUv, 0.0f);

	gScalarWrite[cell] = advectedScalar * gScalarDissipation;
}
