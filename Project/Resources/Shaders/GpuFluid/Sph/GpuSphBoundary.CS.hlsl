#include "GpuSphCommon.hlsli"

void ApplyTangentialBoundaryFriction(inout float3 velocityValue, uint normalAxis)
{
    const float tangentialScale = 1.0f - saturate(gSph.boundaryFriction);
    if (normalAxis != 0u) velocityValue.x *= tangentialScale;
    if (normalAxis != 1u) velocityValue.y *= tangentialScale;
    if (normalAxis != 2u) velocityValue.z *= tangentialScale;
}

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
        ApplyTangentialBoundaryFriction(velocityValue, 0u);
    }
    else if (positionValue.x > maxValue.x)
    {
        positionValue.x = maxValue.x;
        if (velocityValue.x > 0.0f) velocityValue.x *= -gSph.boundaryDamping;
        ApplyTangentialBoundaryFriction(velocityValue, 0u);
    }

    if (positionValue.y < minValue.y)
    {
        positionValue.y = minValue.y;
        if (velocityValue.y < 0.0f) velocityValue.y *= -gSph.boundaryDamping;
        ApplyTangentialBoundaryFriction(velocityValue, 1u);
    }
    else if (positionValue.y > maxValue.y)
    {
        positionValue.y = maxValue.y;
        if (velocityValue.y > 0.0f) velocityValue.y *= -gSph.boundaryDamping;
        ApplyTangentialBoundaryFriction(velocityValue, 1u);
    }

    if (positionValue.z < minValue.z)
    {
        positionValue.z = minValue.z;
        if (velocityValue.z < 0.0f) velocityValue.z *= -gSph.boundaryDamping;
        ApplyTangentialBoundaryFriction(velocityValue, 2u);
    }
    else if (positionValue.z > maxValue.z)
    {
        positionValue.z = maxValue.z;
        if (velocityValue.z > 0.0f) velocityValue.z *= -gSph.boundaryDamping;
        ApplyTangentialBoundaryFriction(velocityValue, 2u);
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
    ResolveBoundary(predictedPosition, velocityValue); // 法線反発と接線摩擦を同時に適用し、壁沿いの不自然な高速滑走を抑える。
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
