#include "GpuFluidCommon.hlsli"

struct GpuFluidEmitterGpuData
{
	float centerCellX;
	float centerCellY;
	float radiusCells;
	float invRadiusCells;

	float velocityX;
	float velocityY;
	float densityRate;
	float temperatureRate;

	float velocityStrength;
	float falloffExponent;
	float padding0;
	float padding1;
};

cbuffer FluidSimulationCB : register(b0)
{
	GpuFluidSimulationConstants gFluid;
};

cbuffer FluidEmitterBatchCB : register(b1)
{
	uint gEmitterCount;
	uint3 gEmitterBatchPadding;
};

Texture2D<float2> gVelocityRead : register(t0);
Texture2D<float> gDensityRead : register(t1);
Texture2D<float> gTemperatureRead : register(t2);
StructuredBuffer<GpuFluidEmitterGpuData> gEmitters : register(t3);
Texture2D<uint> gObstacle : register(t4);

RWTexture2D<float2> gVelocityWrite : register(u0);
RWTexture2D<float> gDensityWrite : register(u1);
RWTexture2D<float> gTemperatureWrite : register(u2);

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
		// Solid CellへSourceを蓄積せず、Collider内部に古い流体値も残さない。
		gVelocityWrite[cell] = 0.0f;
		gDensityWrite[cell] = 0.0f;
		gTemperatureWrite[cell] = 0.0f;
		return;
	}

	const float2 cellCenter = float2(cell) + 0.5f;
	float2 velocity = gVelocityRead.Load(int3(cell, 0));
	float density = gDensityRead.Load(int3(cell, 0));
	float temperature = gTemperatureRead.Load(int3(cell, 0));

	for (uint emitterIndex = 0; emitterIndex < gEmitterCount; ++emitterIndex)
	{
		const GpuFluidEmitterGpuData emitter = gEmitters[emitterIndex];
		const float distanceCells = length(cellCenter - float2(emitter.centerCellX, emitter.centerCellY));
		const float normalizedDistance = saturate(distanceCells * emitter.invRadiusCells);
		const float falloff = pow(
			saturate(1.0f - normalizedDistance),
			max(emitter.falloffExponent, 0.0001f));
		const float sourceDelta = gFluid.deltaTime * falloff;

		velocity += float2(emitter.velocityX, emitter.velocityY) *
			(emitter.velocityStrength * sourceDelta);
		density += emitter.densityRate * sourceDelta;
		temperature += emitter.temperatureRate * sourceDelta;
	}

	gVelocityWrite[cell] = velocity;
	gDensityWrite[cell] = density;
	gTemperatureWrite[cell] = temperature;
}
