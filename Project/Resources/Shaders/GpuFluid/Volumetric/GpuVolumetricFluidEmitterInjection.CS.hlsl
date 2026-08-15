#include "GpuVolumetricFluidCommon.hlsli"

struct GpuVolumetricFluidEmitterGpuData
{
    float centerCellX;
    float centerCellY;
    float centerCellZ;
    float radiusCells;

    float velocityX;
    float velocityY;
    float velocityZ;
    float velocityStrength;

    float densityRate;
    float temperatureRate;
    float falloffExponent;
    float invRadiusCells;

    float padding0;
    float padding1;
    float padding2;
    float padding3;
};

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

cbuffer FluidEmitterBatchCB : register(b1)
{
    uint gEmitterCount;
    uint3 gEmitterBatchPadding;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float> gDensityRead : register(t1);
Texture3D<float> gTemperatureRead : register(t2);
StructuredBuffer<GpuVolumetricFluidEmitterGpuData> gEmitters : register(t3);
Texture3D<uint> gObstacle : register(t4);

RWTexture3D<float4> gVelocityWrite : register(u0);
RWTexture3D<float> gDensityWrite : register(u1);
RWTexture3D<float> gTemperatureWrite : register(u2);

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFluid.gridWidth ||
        dispatchThreadId.y >= gFluid.gridHeight ||
        dispatchThreadId.z >= gFluid.gridDepth)
    {
        return;
    }

    if (gObstacle.Load(int4(dispatchThreadId, 0)) != 0u)
    {
        // Collider内部にSource/古いScalarを蓄積せず、Collider移動時の煙の噴き出しを防ぐ。
        gVelocityWrite[dispatchThreadId] = 0.0f;
        gDensityWrite[dispatchThreadId] = 0.0f;
        gTemperatureWrite[dispatchThreadId] = 0.0f;
        return;
    }

    const float3 cellCenter = float3(dispatchThreadId) + 0.5f;
    float3 velocity = gVelocityRead.Load(int4(dispatchThreadId, 0)).xyz;
    float density = gDensityRead.Load(int4(dispatchThreadId, 0));
    float temperature = gTemperatureRead.Load(int4(dispatchThreadId, 0));

    for (uint emitterIndex = 0; emitterIndex < gEmitterCount; ++emitterIndex)
    {
        const GpuVolumetricFluidEmitterGpuData emitter = gEmitters[emitterIndex];
        const float3 emitterCenter = float3(
            emitter.centerCellX,
            emitter.centerCellY,
            emitter.centerCellZ);
        const float distanceCells = length(cellCenter - emitterCenter);
        const float normalizedDistance = saturate(distanceCells * emitter.invRadiusCells);
        const float falloff = pow(
            saturate(1.0f - normalizedDistance),
            max(emitter.falloffExponent, 0.0001f));
        const float sourceDelta = gFluid.deltaTime * falloff;

        velocity += float3(emitter.velocityX, emitter.velocityY, emitter.velocityZ) *
            (emitter.velocityStrength * sourceDelta);
        density += emitter.densityRate * sourceDelta;
        temperature += emitter.temperatureRate * sourceDelta;
    }

    gVelocityWrite[dispatchThreadId] = float4(velocity, 0.0f);
    gDensityWrite[dispatchThreadId] = density;
    gTemperatureWrite[dispatchThreadId] = temperature;
}
