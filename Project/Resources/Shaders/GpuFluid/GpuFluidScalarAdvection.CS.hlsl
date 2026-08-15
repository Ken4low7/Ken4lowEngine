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
Texture2D<uint> gObstacle : register(t2);
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
	if (gObstacle.Load(int3(cell, 0)) != 0u)
	{
		gScalarWrite[cell] = 0.0f;
		return;
	}

	const float2 uv = GpuFluidCellToUv(cell, gFluid);
	const float2 velocity = gVelocityRead.SampleLevel(gLinearClampSampler, uv, 0.0f);
	const float2 backtraceUvOffset =
		velocity * gFluid.deltaTime * gFluid.invCellSize *
		float2(gFluid.invGridWidth, gFluid.invGridHeight);
	float2 sourceUv = GpuFluidClampUvToCellCenters(uv - backtraceUvOffset, gFluid);
	const uint2 sourceCell = min(
		uint2(sourceUv * float2(gFluid.gridWidth, gFluid.gridHeight)),
		uint2(gFluid.gridWidth - 1u, gFluid.gridHeight - 1u));
	if (gObstacle.Load(int3(sourceCell, 0)) != 0u)
	{
		sourceUv = uv; // Density/TemperatureがSolidの向こうから直接引き込まれないよう逆追跡を止める。
	}

	const float advectedScalar = gScalarRead.SampleLevel(gLinearClampSampler, sourceUv, 0.0f);
	gScalarWrite[cell] = advectedScalar * gScalarDissipation;
}
