#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float> gDensityRead : register(t1);
Texture3D<float> gTemperatureRead : register(t2);
RWTexture3D<float4> gVelocityWrite : register(u0);

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gFluid.gridWidth ||
        dispatchThreadId.y >= gFluid.gridHeight ||
        dispatchThreadId.z >= gFluid.gridDepth)
    {
        return;
    }

    const int3 cell = int3(dispatchThreadId);
    const float density = gDensityRead.Load(int4(cell, 0));
    const float temperature = gTemperatureRead.Load(int4(cell, 0));
    float3 velocity = gVelocityRead.Load(int4(cell, 0)).xyz;

    const float buoyancyForce =
        gFluid.buoyancy * (temperature - gFluid.ambientTemperature) -
        gFluid.smokeWeight * density;
    velocity.y += buoyancyForce * gFluid.deltaTime;

    // Force適用中も閉じたVolume外周を破らないよう、各面の法線速度だけ0へ戻す。
    if (dispatchThreadId.x == 0 || dispatchThreadId.x + 1 >= gFluid.gridWidth)
    {
        velocity.x = 0.0f;
    }
    if (dispatchThreadId.y == 0 || dispatchThreadId.y + 1 >= gFluid.gridHeight)
    {
        velocity.y = 0.0f;
    }
    if (dispatchThreadId.z == 0 || dispatchThreadId.z + 1 >= gFluid.gridDepth)
    {
        velocity.z = 0.0f;
    }

    gVelocityWrite[dispatchThreadId] = float4(velocity, 0.0f);
}
