#include "GpuSphCommon.hlsli"

void ResolveBoundary(inout float3 positionValue, inout float3 velocityValue)
{
    const float radius = max(gSph.spawnSpacing * 0.5f, 0.001f);
    const float3 margin = float3(radius, radius, radius);
    const float3 minValue = gSph.boundaryMin + margin;
    const float3 maxValue = gSph.boundaryMax - margin;

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

    float3 predictedPosition = gParticles[index].predictedPosition;
    float3 velocityValue = gParticles[index].velocity;
    // W5.3では予測位置をDomain内へ収め、外向き速度だけを反射する。
    ResolveBoundary(predictedPosition, velocityValue);
    gParticles[index].predictedPosition = predictedPosition;
    gParticles[index].velocity = velocityValue;
}

[numthreads(128, 1, 1)]
void ConstrainPosition(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    float3 positionValue = gParticles[index].position;
    float3 velocityValue = gParticles[index].velocity;
    ResolveBoundary(positionValue, velocityValue);
    gParticles[index].position = positionValue;
    gParticles[index].predictedPosition = positionValue;
    gParticles[index].velocity = velocityValue;
}
