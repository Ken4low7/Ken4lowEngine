#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gVorticityRead : register(t1);
Texture2D<uint> gObstacle : register(t3);
RWTexture2D<float2> gVelocityWrite : register(u0);

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
		gVelocityWrite[dispatchThreadId.xy] = 0.0f;
		return;
	}

	const int2 leftCell = GpuFluidClampCell(cell + int2(-1, 0), gFluid);
	const int2 rightCell = GpuFluidClampCell(cell + int2(1, 0), gFluid);
	const int2 bottomCell = GpuFluidClampCell(cell + int2(0, -1), gFluid);
	const int2 topCell = GpuFluidClampCell(cell + int2(0, 1), gFluid);
	const float omega = gVorticityRead.Load(int3(cell, 0));
	const float centerMagnitude = abs(omega);

	const bool solidLeft = gObstacle.Load(int3(leftCell, 0)) != 0u;
	const bool solidRight = gObstacle.Load(int3(rightCell, 0)) != 0u;
	const bool solidBottom = gObstacle.Load(int3(bottomCell, 0)) != 0u;
	const bool solidTop = gObstacle.Load(int3(topCell, 0)) != 0u;
	const float omegaLeft = solidLeft ? centerMagnitude : abs(gVorticityRead.Load(int3(leftCell, 0)));
	const float omegaRight = solidRight ? centerMagnitude : abs(gVorticityRead.Load(int3(rightCell, 0)));
	const float omegaBottom = solidBottom ? centerMagnitude : abs(gVorticityRead.Load(int3(bottomCell, 0)));
	const float omegaTop = solidTop ? centerMagnitude : abs(gVorticityRead.Load(int3(topCell, 0)));

	float2 gradient = float2(
		omegaRight - omegaLeft,
		omegaTop - omegaBottom) * (0.5f * gFluid.invCellSize);
	const float gradientLength = length(gradient);
	const float2 normal = gradientLength > 1.0e-5f ? gradient / gradientLength : float2(0.0f, 0.0f);
	const float2 confinementForce =
		gFluid.vorticityStrength * float2(normal.y, -normal.x) * omega;
	float2 velocity = gVelocityRead.Load(int3(cell, 0)) + confinementForce * gFluid.deltaTime;

	// Force Pass単体でもSolid面へ押し込まないよう、隣接壁の法線成分を先に落としておく。
	if (solidLeft || solidRight) velocity.x = 0.0f;
	if (solidBottom || solidTop) velocity.y = 0.0f;
	gVelocityWrite[dispatchThreadId.xy] = velocity;
}
