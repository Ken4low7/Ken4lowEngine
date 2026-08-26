#ifndef KEN4LOW_GPU_SPH_COMMON_HLSLI
#define KEN4LOW_GPU_SPH_COMMON_HLSLI

struct GpuSphParticle
{
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 predictedPosition;
    float padding;
};

struct GpuSphSimulationConstants
{
    uint activeParticleCount;
    float deltaTime;
    float particleMass;
    float smoothingRadius;

    float targetDensity;
    float pressureStiffness;
    float viscosityStrength;
    float boundaryDamping;

    float3 gravity;
    float padding0;

    float3 boundaryMin;
    float padding1;

    float3 boundaryMax;
    float padding2;

    float3 spawnOrigin;
    float spawnSpacing;

    uint spawnDimX;
    uint spawnDimY;
    uint spawnDimZ;
    uint padding3;
};

cbuffer GpuSphSimulationCB : register(b0)
{
    GpuSphSimulationConstants gSph;
};

RWStructuredBuffer<GpuSphParticle> gParticles : register(u0);
RWStructuredBuffer<float4> gScratch : register(u1);

// W5では全Passで同じ粒子数とDescriptor契約を共有する。
bool GpuSphIsActiveParticle(uint index)
{
    return index < gSph.activeParticleCount;
}

#endif // KEN4LOW_GPU_SPH_COMMON_HLSLI
