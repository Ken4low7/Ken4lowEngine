#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocityRead : register(t0);
Texture3D<float4> gVorticityRead : register(t1);
Texture3D<uint> gObstacle : register(t3);
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
    if (gObstacle.Load(int4(cell, 0)) != 0u)
    {
        gVelocityWrite[dispatchThreadId] = 0.0f;
        return;
    }

    const float3 omega = gVorticityRead.Load(int4(cell, 0)).xyz;
    const float centerMagnitude = length(omega);

    float magnitudeLeft = centerMagnitude;
    float magnitudeRight = centerMagnitude;
    float magnitudeBottom = centerMagnitude;
    float magnitudeTop = centerMagnitude;
    float magnitudeBack = centerMagnitude;
    float magnitudeFront = centerMagnitude;

    const int3 leftCell = cell + int3(-1, 0, 0);
    const int3 rightCell = cell + int3(1, 0, 0);
    const int3 bottomCell = cell + int3(0, -1, 0);
    const int3 topCell = cell + int3(0, 1, 0);
    const int3 backCell = cell + int3(0, 0, -1);
    const int3 frontCell = cell + int3(0, 0, 1);

    bool blockedLeft = cell.x <= 0;
    bool blockedRight = cell.x + 1 >= int(gFluid.gridWidth);
    bool blockedBottom = cell.y <= 0;
    bool blockedTop = cell.y + 1 >= int(gFluid.gridHeight);
    bool blockedBack = cell.z <= 0;
    bool blockedFront = cell.z + 1 >= int(gFluid.gridDepth);

    if (!blockedLeft)
    {
        blockedLeft = gObstacle.Load(int4(leftCell, 0)) != 0u;
        if (!blockedLeft) magnitudeLeft = length(gVorticityRead.Load(int4(leftCell, 0)).xyz);
    }
    if (!blockedRight)
    {
        blockedRight = gObstacle.Load(int4(rightCell, 0)) != 0u;
        if (!blockedRight) magnitudeRight = length(gVorticityRead.Load(int4(rightCell, 0)).xyz);
    }
    if (!blockedBottom)
    {
        blockedBottom = gObstacle.Load(int4(bottomCell, 0)) != 0u;
        if (!blockedBottom) magnitudeBottom = length(gVorticityRead.Load(int4(bottomCell, 0)).xyz);
    }
    if (!blockedTop)
    {
        blockedTop = gObstacle.Load(int4(topCell, 0)) != 0u;
        if (!blockedTop) magnitudeTop = length(gVorticityRead.Load(int4(topCell, 0)).xyz);
    }
    if (!blockedBack)
    {
        blockedBack = gObstacle.Load(int4(backCell, 0)) != 0u;
        if (!blockedBack) magnitudeBack = length(gVorticityRead.Load(int4(backCell, 0)).xyz);
    }
    if (!blockedFront)
    {
        blockedFront = gObstacle.Load(int4(frontCell, 0)) != 0u;
        if (!blockedFront) magnitudeFront = length(gVorticityRead.Load(int4(frontCell, 0)).xyz);
    }

    const float3 magnitudeGradient = 0.5f * gFluid.invCellSize * float3(
        magnitudeRight - magnitudeLeft,
        magnitudeTop - magnitudeBottom,
        magnitudeFront - magnitudeBack);
    const float gradientLength = length(magnitudeGradient);
    const float3 normal = gradientLength > 1.0e-5f
        ? magnitudeGradient / gradientLength
        : float3(0.0f, 0.0f, 0.0f);

    // Solid neighborはcenterMagnitude扱いにして、壁境界そのものを人工的な|omega|勾配にしない。
    const float3 confinementForce =
        gFluid.vorticityStrength * cross(normal, omega);
    float3 velocity =
        gVelocityRead.Load(int4(cell, 0)).xyz + confinementForce * gFluid.deltaTime;

    if (blockedLeft || blockedRight)
    {
        velocity.x = 0.0f;
    }
    if (blockedBottom || blockedTop)
    {
        velocity.y = 0.0f;
    }
    if (blockedBack || blockedFront)
    {
        velocity.z = 0.0f;
    }

    gVelocityWrite[dispatchThreadId] = float4(velocity, 0.0f);
}
