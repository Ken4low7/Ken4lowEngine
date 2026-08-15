#include "GpuFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gDensityRead : register(t1);
Texture2D<float> gTemperatureRead : register(t2);
RWTexture2D<float2> gVelocityWrite : register(u0);

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= gFluid.gridWidth || dispatchThreadId.y >= gFluid.gridHeight)
	{
		return;
	}

	const int2 cell = int2(dispatchThreadId.xy);
	const float density = gDensityRead.Load(int3(cell, 0));
	const float temperature = gTemperatureRead.Load(int3(cell, 0));
	float2 velocity = gVelocityRead.Load(int3(cell, 0));

	// 熱による上昇力から煙密度の重さを引き、DensityとTemperatureを速度場へフィードバックする。
	const float buoyancyForce =
		gFluid.buoyancy * (temperature - gFluid.ambientTemperature) -
		gFluid.smokeWeight * density;
	velocity.y += buoyancyForce * gFluid.deltaTime;
	gVelocityWrite[dispatchThreadId.xy] = velocity;
}
