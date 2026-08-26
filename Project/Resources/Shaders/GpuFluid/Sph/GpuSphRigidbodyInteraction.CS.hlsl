struct GpuSphParticle
{
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 predictedPosition;
    float padding;
};

struct GpuSphRigidbodyProxy
{
    uint shapeType;
    uint bodyIndex;
    float restitution;
    float friction;

    float3 center;
    float radius;

    float3 halfSize;
    float capsuleHalfLength;

    float3 axisX;
    float padding0;
    float3 axisY;
    float padding1;
    float3 axisZ;
    float padding2;

    float3 bodyCenter;
    float padding3;
    float3 linearVelocity;
    float padding4;
    float3 angularVelocity;
    float padding5;
};

struct GpuSphRigidbodyReaction
{
    int impulseX;
    int impulseY;
    int impulseZ;
    int padding0;
    int torqueX;
    int torqueY;
    int torqueZ;
    int padding1;
};

cbuffer GpuSphRigidbodyInteractionCB : register(b0)
{
    uint gActiveParticleCount;
    uint gProxyCount;
    uint gBodyCount;
    uint gAccumulateReaction;

    float gParticleMass;
    float gParticleRadius;
    float gCouplingStrength;
    float gCollisionRestitution;

    float gCollisionFriction;
    float gDeltaTime;
    float gImpulseScale;
    float gPadding0;
};

RWStructuredBuffer<GpuSphParticle> gParticles : register(u0);
StructuredBuffer<GpuSphRigidbodyProxy> gRigidbodies : register(t0);
RWStructuredBuffer<GpuSphRigidbodyReaction> gReactions : register(u1);

static const uint kShapeSphere = 0u;
static const uint kShapeAabb = 1u;
static const uint kShapeObb = 2u;
static const uint kShapeCapsule = 3u;
static const uint kInvalidBodyIndex = 0xffffffffu;

float SignNonZero(float value)
{
    return value >= 0.0f ? 1.0f : -1.0f;
}

bool ResolveBox(
    float3 worldPosition,
    GpuSphRigidbodyProxy proxy,
    bool oriented,
    out float3 normal,
    out float penetration)
{
    const float3 delta = worldPosition - proxy.center;
    float3 localPosition = delta;
    if (oriented)
    {
        localPosition = float3(
            dot(delta, proxy.axisX),
            dot(delta, proxy.axisY),
            dot(delta, proxy.axisZ));
    }

    const float3 expandedHalfSize = max(proxy.halfSize, 0.0f.xxx) + gParticleRadius.xxx;
    const float3 remaining = expandedHalfSize - abs(localPosition);
    if (remaining.x < 0.0f || remaining.y < 0.0f || remaining.z < 0.0f)
    {
        normal = 0.0f.xxx;
        penetration = 0.0f;
        return false;
    }

    float3 localNormal = float3(SignNonZero(localPosition.x), 0.0f, 0.0f);
    penetration = remaining.x;
    if (remaining.y < penetration)
    {
        penetration = remaining.y;
        localNormal = float3(0.0f, SignNonZero(localPosition.y), 0.0f);
    }
    if (remaining.z < penetration)
    {
        penetration = remaining.z;
        localNormal = float3(0.0f, 0.0f, SignNonZero(localPosition.z));
    }

    normal = oriented
        ? normalize(proxy.axisX * localNormal.x + proxy.axisY * localNormal.y + proxy.axisZ * localNormal.z)
        : localNormal;
    return true;
}

bool ResolveSphere(
    float3 worldPosition,
    GpuSphRigidbodyProxy proxy,
    out float3 normal,
    out float penetration)
{
    const float3 delta = worldPosition - proxy.center;
    const float distanceValue = length(delta);
    const float limit = max(proxy.radius, 0.0f) + gParticleRadius;
    if (distanceValue >= limit)
    {
        normal = 0.0f.xxx;
        penetration = 0.0f;
        return false;
    }

    normal = distanceValue > 1.0e-5f ? delta / distanceValue : float3(0.0f, 1.0f, 0.0f);
    penetration = limit - distanceValue;
    return true;
}

bool ResolveCapsule(
    float3 worldPosition,
    GpuSphRigidbodyProxy proxy,
    out float3 normal,
    out float penetration)
{
    const float3 axis = normalize(length(proxy.axisY) > 1.0e-5f ? proxy.axisY : float3(0.0f, 1.0f, 0.0f));
    const float3 start = proxy.center - axis * max(proxy.capsuleHalfLength, 0.0f);
    const float3 end = proxy.center + axis * max(proxy.capsuleHalfLength, 0.0f);
    const float3 segment = end - start;
    const float segmentLengthSquared = max(dot(segment, segment), 1.0e-6f);
    const float t = saturate(dot(worldPosition - start, segment) / segmentLengthSquared);
    const float3 closest = start + segment * t;
    const float3 delta = worldPosition - closest;
    const float distanceValue = length(delta);
    const float limit = max(proxy.radius, 0.0f) + gParticleRadius;
    if (distanceValue >= limit)
    {
        normal = 0.0f.xxx;
        penetration = 0.0f;
        return false;
    }

    normal = distanceValue > 1.0e-5f ? delta / distanceValue : float3(0.0f, 1.0f, 0.0f);
    penetration = limit - distanceValue;
    return true;
}

bool ResolveShape(
    float3 worldPosition,
    GpuSphRigidbodyProxy proxy,
    out float3 normal,
    out float penetration)
{
    if (proxy.shapeType == kShapeSphere)
    {
        return ResolveSphere(worldPosition, proxy, normal, penetration);
    }
    if (proxy.shapeType == kShapeAabb)
    {
        return ResolveBox(worldPosition, proxy, false, normal, penetration);
    }
    if (proxy.shapeType == kShapeObb)
    {
        return ResolveBox(worldPosition, proxy, true, normal, penetration);
    }
    if (proxy.shapeType == kShapeCapsule)
    {
        return ResolveCapsule(worldPosition, proxy, normal, penetration);
    }

    normal = 0.0f.xxx;
    penetration = 0.0f;
    return false;
}

void AccumulateReaction(uint bodyIndex, float3 impulse, float3 torque)
{
    if (gAccumulateReaction == 0u || bodyIndex == kInvalidBodyIndex || bodyIndex >= gBodyCount)
    {
        return;
    }

    const float3 scaledImpulse = clamp(impulse * gImpulseScale, -2000000000.0f.xxx, 2000000000.0f.xxx);
    const float3 scaledTorque = clamp(torque * gImpulseScale, -2000000000.0f.xxx, 2000000000.0f.xxx);

    InterlockedAdd(gReactions[bodyIndex].impulseX, (int)round(scaledImpulse.x));
    InterlockedAdd(gReactions[bodyIndex].impulseY, (int)round(scaledImpulse.y));
    InterlockedAdd(gReactions[bodyIndex].impulseZ, (int)round(scaledImpulse.z));
    InterlockedAdd(gReactions[bodyIndex].torqueX, (int)round(scaledTorque.x));
    InterlockedAdd(gReactions[bodyIndex].torqueY, (int)round(scaledTorque.y));
    InterlockedAdd(gReactions[bodyIndex].torqueZ, (int)round(scaledTorque.z));
}

[numthreads(64, 1, 1)]
void ClearReactions(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= gBodyCount)
    {
        return;
    }

    GpuSphRigidbodyReaction reaction;
    reaction.impulseX = 0;
    reaction.impulseY = 0;
    reaction.impulseZ = 0;
    reaction.padding0 = 0;
    reaction.torqueX = 0;
    reaction.torqueY = 0;
    reaction.torqueZ = 0;
    reaction.padding1 = 0;
    gReactions[index] = reaction;
}

[numthreads(128, 1, 1)]
void ResolveParticles(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= gActiveParticleCount)
    {
        return;
    }

    GpuSphParticle particle = gParticles[particleIndex];
    float3 positionValue = particle.position;
    float3 velocityValue = particle.velocity;

    [loop]
    for (uint proxyIndex = 0u; proxyIndex < gProxyCount; ++proxyIndex)
    {
        const GpuSphRigidbodyProxy proxy = gRigidbodies[proxyIndex];
        float3 normal;
        float penetration;
        if (!ResolveShape(positionValue, proxy, normal, penetration))
        {
            continue;
        }

        positionValue += normal * (penetration + 1.0e-4f);

        const float3 contactPoint = positionValue - normal * gParticleRadius;
        const float3 bodySurfaceVelocity = proxy.linearVelocity + cross(proxy.angularVelocity, contactPoint - proxy.bodyCenter);
        const float3 oldVelocity = velocityValue;
        float3 relativeVelocity = velocityValue - bodySurfaceVelocity;
        const float normalSpeed = dot(relativeVelocity, normal);
        if (normalSpeed < 0.0f)
        {
            const float restitution = saturate(max(gCollisionRestitution, proxy.restitution));
            const float friction = saturate(max(gCollisionFriction, proxy.friction));
            const float3 tangentVelocity = relativeVelocity - normal * normalSpeed;
            relativeVelocity -= normal * ((1.0f + restitution) * normalSpeed);
            relativeVelocity -= tangentVelocity * friction;
            velocityValue = bodySurfaceVelocity + relativeVelocity;
        }

        const float3 particleImpulse = (velocityValue - oldVelocity) * max(gParticleMass, 0.0f);
        const float3 bodyImpulse = -particleImpulse * max(gCouplingStrength, 0.0f);
        const float3 bodyTorqueImpulse = cross(contactPoint - proxy.bodyCenter, bodyImpulse);
        AccumulateReaction(proxy.bodyIndex, bodyImpulse, bodyTorqueImpulse);
    }

    particle.position = positionValue;
    particle.velocity = velocityValue;
    particle.predictedPosition = positionValue + velocityValue * max(gDeltaTime, 0.0f);
    gParticles[particleIndex] = particle;
}
