#include "GpuVolumetricFluidCommon.hlsli"

cbuffer FluidSimulationCB : register(b0)
{
    GpuVolumetricFluidSimulationConstants gFluid;
};

Texture3D<float4> gVelocity : register(t0);
RWTexture3D<float> gDivergence : register(u0);

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
    float3 velocityLeft = 0.0f;
    float3 velocityRight = 0.0f;
    float3 velocityBottom = 0.0f;
    float3 velocityTop = 0.0f;
    float3 velocityBack = 0.0f;
    float3 velocityFront = 0.0f;

    if (cell.x > 0)
    {
        velocityLeft = gVelocity.Load(int4(cell + int3(-1, 0, 0), 0)).xyz;
    }
    if (cell.x + 1 < int(gFluid.gridWidth))
    {
        velocityRight = gVelocity.Load(int4(cell + int3(1, 0, 0), 0)).xyz;
    }
    if (cell.y > 0)
    {
        velocityBottom = gVelocity.Load(int4(cell + int3(0, -1, 0), 0)).xyz;
    }
    if (cell.y + 1 < int(gFluid.gridHeight))
    {
        velocityTop = gVelocity.Load(int4(cell + int3(0, 1, 0), 0)).xyz;
    }
    if (cell.z > 0)
    {
        velocityBack = gVelocity.Load(int4(cell + int3(0, 0, -1), 0)).xyz;
    }
    if (cell.z + 1 < int(gFluid.gridDepth))
    {
        velocityFront = gVelocity.Load(int4(cell + int3(0, 0, 1), 0)).xyz;
    }

    // Volume外側の速度を0として扱い、閉じたDomainの6面から外へFluxを流さない。
    const float divergence = 0.5f * gFluid.invCellSize *
        ((velocityRight.x - velocityLeft.x) +
         (velocityTop.y - velocityBottom.y) +
         (velocityFront.z - velocityBack.z));
    gDivergence[dispatchThreadId] = divergence;
}
