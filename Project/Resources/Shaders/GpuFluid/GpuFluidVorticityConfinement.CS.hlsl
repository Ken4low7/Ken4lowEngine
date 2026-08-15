#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gVorticityRead : register(t1);
RWTexture2D<float2> gVelocityWrite : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const int2 cell = int2(dispatchThreadId.xy);
	const float omegaLeft = abs(gVorticityRead.Load(int3(GpuFluidClampCell(cell + int2(-1, 0), gFluid), 0)));
	const float omegaRight = abs(gVorticityRead.Load(int3(GpuFluidClampCell(cell + int2(1, 0), gFluid), 0)));
	const float omegaBottom = abs(gVorticityRead.Load(int3(GpuFluidClampCell(cell + int2(0, -1), gFluid), 0)));
	const float omegaTop = abs(gVorticityRead.Load(int3(GpuFluidClampCell(cell + int2(0, 1), gFluid), 0)));
	const float omega = gVorticityRead.Load(int3(cell, 0));

	float2 gradient = float2(
		omegaRight - omegaLeft,
		omegaTop - omegaBottom) * (0.5f * gFluid.invCellSize);
	const float gradientLength = length(gradient);
	const float2 normal = gradientLength > 1.0e-5f ? gradient / gradientLength : float2(0.0f, 0.0f);

	// |curl|勾配に直交する力を加え、数値拡散で失われる小さな渦を補う。
	const float2 confinementForce =
		gFluid.vorticityStrength * float2(normal.y, -normal.x) * omega;
	gVelocityWrite[dispatchThreadId.xy] =
		gVelocityRead.Load(int3(cell, 0)) + confinementForce * gFluid.deltaTime;
}
