#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float4> gVorticityRead : register(t1);
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
    const float3 omega = gVorticityRead.Load(int4(cell, 0)).xyz;
    const float centerMagnitude = length(omega);

    float magnitudeLeft = centerMagnitude;
    float magnitudeRight = centerMagnitude;
    float magnitudeBottom = centerMagnitude;
    float magnitudeTop = centerMagnitude;
    float magnitudeBack = centerMagnitude;
    float magnitudeFront = centerMagnitude;

    if (cell.x > 0)
    {
        magnitudeLeft = length(gVorticityRead.Load(int4(cell + int3(-1, 0, 0), 0)).xyz);
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        magnitudeRight = length(gVorticityRead.Load(int4(cell + int3(1, 0, 0), 0)).xyz);
    }
    if (cell.y > 0)
    {
        magnitudeBottom = length(gVorticityRead.Load(int4(cell + int3(0, -1, 0), 0)).xyz);
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        magnitudeTop = length(gVorticityRead.Load(int4(cell + int3(0, 1, 0), 0)).xyz);
    }
    if (cell.z > 0)
    {
        magnitudeBack = length(gVorticityRead.Load(int4(cell + int3(0, 0, -1), 0)).xyz);
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        magnitudeFront = length(gVorticityRead.Load(int4(cell + int3(0, 0, 1), 0)).xyz);
    }

    const float3 magnitudeGradient = 0.5f * gFluid.invCellSize * float3(
        magnitudeRight - magnitudeLeft,
        magnitudeTop - magnitudeBottom,
        magnitudeFront - magnitudeBack);
    const float gradientLength = length(magnitudeGradient);
    const float3 normal = gradientLength > 1.0e-5f
        ? magnitudeGradient / gradientLength
        : float3(0.0f, 0.0f, 0.0f);

    // N×ωで散逸しやすい小スケールの渦を速度場へ戻す。
    const float3 confinementForce =
        gFluid.vorticityStrength * cross(normal, omega);
    float3 velocity =
        gVelocityRead.Load(int4(cell, 0)).xyz + confinementForce * gFluid.deltaTime;

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
