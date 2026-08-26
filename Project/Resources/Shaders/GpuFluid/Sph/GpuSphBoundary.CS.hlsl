#include "GpuSphCommon.hlsli"

void ResolveBoundary(inout float3 positionValue, inout float3 velocityValue)
{
    const float radius = max(gSph.spawnSpacing * 0.5f, 0.001f);
    const float3 minValue = gSph.boundaryMin + radius.xxx;
    const float3 maxValue = gSph.boundaryMax - radius.xxx;

    if (positionValue.x < minValue.x)
    {
        positionValue.x = minValue.x;
        if (velocityValue.x < 0.0f) velocityValue.x *= -gSph.boundaryDamping;
    }
    else if (positionValue.x > maxValue.x)
    {
        positionValue.x = maxValue.x;
        if (velocityValue.x > 0.0f) velocityValue.x *= -gSph.boundaryDamping;
    }

    if (positionValue.y < minValue.y)
    {
        positionValue.y = minValue.y;
        if (velocityValue.y < 0.0f) velocityValue.y *= -gSph.boundaryDamping;
    }
    else if (positionValue.y > maxValue.y)
    {
        positionValue.y = maxValue.y;
        if (velocityValue.y > 0.0f) velocityValue.y *= -gSph.boundaryDamping;
    }

    if (positionValue.z < minValue.z)
    {
        positionValue.z = minValue.z;
        if (velocityValue.z < 0.0f) velocityValue.z *= -gSph.boundaryDamping;
    }
    else if (positionValue.z > maxValue.z)
    {
        positionValue.z = maxValue.z;
        if (velocityValue.z > 0.0f) velocityValue.z *= -gSph.boundaryDamping;
    }
}

[numthreads(128, 1, 1)]
void ConstrainPredicted(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    // W5.3では予測位置をDomain内へ収め、外向き速度だけを反射する。
    ResolveBoundary(particle.predictedPosition, particle.velocity);
    gParticles[index] = particle;
}

[numthreads(128, 1, 1)]
void ConstrainPosition(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    ResolveBoundary(particle.position, particle.velocity);
    particle.predictedPosition = particle.position;
    gParticles[index] = particle;
}
