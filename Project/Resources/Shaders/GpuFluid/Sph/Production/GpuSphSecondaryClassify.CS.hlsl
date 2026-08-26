struct GpuSphParticle
{
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 predictedPosition;
    float padding;
};

cbuffer SecondaryClassifyCB : register(b0)
{
    uint gActiveParticleCount;
    float gTargetDensity;
    float gSpraySpeedThreshold;
    float gFoamSpeedThreshold;
    float gFreeSurfaceDensityRatio;
    float gBubbleDensityRatio;
    float2 gPadding;
};

StructuredBuffer<GpuSphParticle> gParticles : register(t0);
RWStructuredBuffer<uint> gCounters : register(u0);

[numthreads(1, 1, 1)]
void ClearCounters(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x == 0u)
    {
        gCounters[0] = 0u;
        gCounters[1] = 0u;
        gCounters[2] = 0u;
        gCounters[3] = 0u;
    }
}

[numthreads(128, 1, 1)]
void Classify(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= gActiveParticleCount)
    {
        return;
    }

    const GpuSphParticle particle = gParticles[index];
    const float speed = length(particle.velocity);
    const float densityRatio = particle.density / max(gTargetDensity, 1.0e-5f);
    const bool freeSurface = densityRatio <= gFreeSurfaceDensityRatio;

    // W10.4: Secondary粒子をSpawnする前段として、Primary SPHから候補だけをGPUで分類する。
    if (freeSurface && speed >= gSpraySpeedThreshold)
    {
        InterlockedAdd(gCounters[0], 1u);
    }
    if (freeSurface && speed >= gFoamSpeedThreshold)
    {
        InterlockedAdd(gCounters[1], 1u);
    }
    if (!freeSurface && densityRatio >= gBubbleDensityRatio && particle.velocity.y > 0.0f)
    {
        InterlockedAdd(gCounters[2], 1u);
    }
    InterlockedAdd(gCounters[3], 1u);
}
