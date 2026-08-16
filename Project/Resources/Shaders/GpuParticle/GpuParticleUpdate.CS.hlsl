#include "GpuParticleData.hlsli"
#include "GpuParticleTypeParams.hlsli"

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

ConstantBuffer<PerFrame> gPerFrame : register(b2);

float SampleScalarLut(float4 lut, float t)
{
    float position = saturate(t) * 3.0f;
    uint index = min((uint)floor(position), 2u);
    float localT = frac(position);
    float a = index == 0u ? lut.x : (index == 1u ? lut.y : lut.z);
    float b = index == 0u ? lut.y : (index == 1u ? lut.z : lut.w);
    return lerp(a, b, localT);
}

float4 SampleColorGradient(Particle p, float t)
{
    float position = saturate(t) * 3.0f;
    uint index = min((uint)floor(position), 2u);
    float localT = frac(position);
    float4 a = index == 0u ? p.colorGradientLut0 : (index == 1u ? p.colorGradientLut1 : p.colorGradientLut2);
    float4 b = index == 0u ? p.colorGradientLut1 : (index == 1u ? p.colorGradientLut2 : p.colorGradientLut3);
    return lerp(a, b, localT);
}

float3 EvaluateNoiseAcceleration(Particle p)
{
    if (abs(p.noiseStrength) <= 1e-6f || p.noiseFrequency <= 1e-6f) return 0.0f;
    float3 cell = floor((p.translate + gPerFrame.time.xxx) * p.noiseFrequency);
    return (GPURand3(cell + 17.37f) * 2.0f - 1.0f) * p.noiseStrength;
}

float3 EvaluateVortexAcceleration(Particle p)
{
    if (abs(p.vortexStrength) <= 1e-6f) return 0.0f;
    float axisLength = length(p.vortexAxis);
    float3 relative = p.translate - p.forceOrigin;
    if (axisLength <= 1e-6f || length(relative) <= 1e-6f) return 0.0f;
    float3 tangent = cross(p.vortexAxis / axisLength, relative);
    float tangentLength = length(tangent);
    return tangentLength > 1e-6f ? tangent / tangentLength * p.vortexStrength : 0.0f;
}

float3 EvaluateAttractorAcceleration(Particle p)
{
    if (abs(p.attractorStrength) <= 1e-6f) return 0.0f;
    float3 delta = p.attractorPosition - p.translate;
    float distanceToAttractor = length(delta);
    if (distanceToAttractor <= 1e-6f) return 0.0f;
    if (p.attractorRadius > 0.0f && distanceToAttractor > p.attractorRadius) return 0.0f;
    float falloff = p.attractorRadius > 0.0f
        ? saturate(1.0f - distanceToAttractor / max(p.attractorRadius, 1e-5f))
        : 1.0f;
    return delta / distanceToAttractor * p.attractorStrength * falloff;
}

bool TryAllocateParticle(out uint particleIndex)
{
    int top;
    InterlockedAdd(gFreeListIndex[0], -1, top);
    if (0 <= top && top < (int)kMaxParticleCount)
    {
        particleIndex = gFreeList[top];
        return true;
    }
    InterlockedAdd(gFreeListIndex[0], 1, top);
    particleIndex = 0u;
    return false;
}

void ReturnParticleToFreeList(uint particleIndex, inout Particle p)
{
    p.color.a = 0.0f;
    p.scale = 0.0f;
    p.lifeTime = 0.0f;
    p.currentTime = 0.0f;
    gParticles[particleIndex] = p;

    int oldTop;
    InterlockedAdd(gFreeListIndex[0], 1, oldTop);
    uint newTop = (uint)(oldTop + 1);
    if (newTop < kMaxParticleCount)
    {
        gFreeList[newTop] = particleIndex;
    }
    else
    {
        InterlockedAdd(gFreeListIndex[0], -1, oldTop);
    }
}

float3 SafeEventDirection(float3 value, float3 fallbackDirection)
{
    float len = length(value);
    if (len > 1e-6f) return value / len;
    float fallbackLength = length(fallbackDirection);
    return fallbackLength > 1e-6f ? fallbackDirection / fallbackLength : float3(0.0f, 1.0f, 0.0f);
}

void SpawnSubEmitter(Particle parent, uint eventBit, float3 eventPosition, float3 eventNormal, uint sourceParticleIndex)
{
    if ((parent.eventMask & eventBit) == 0u || (parent.subEmitterEventMask & eventBit) == 0u || parent.subEmitterCount == 0u)
        return;

    uint spawnCount = min(parent.subEmitterCount, 64u);
    for (uint i = 0u; i < spawnCount; ++i)
    {
        uint childIndex;
        if (!TryAllocateParticle(childIndex)) break;

        float3 seed = float3(
            (float)(sourceParticleIndex * 131u + i * 17u),
            gPerFrame.time * 71.37f,
            (float)(parent.type ^ eventBit) * 0.03125f);
        float3 randomDirection = GPURand3(seed + 23.71f) * 2.0f - 1.0f;
        float3 baseDirection = eventBit == GPU_PARTICLE_EVENT_COLLISION ? eventNormal : randomDirection;
        float3 direction = SafeEventDirection(baseDirection + randomDirection * parent.subEmitterSpread, randomDirection);

        Particle child = (Particle)0;
        child.translate = eventPosition + direction * max(parent.collisionParticleRadius, 0.001f);
        child.velocity = parent.velocity * parent.subEmitterInheritVelocity + direction * parent.subEmitterSpeed;
        child.lifeTime = max(parent.subEmitterLifeTime, 0.01f);
        child.currentTime = 0.0f;
        child.type = parent.type;
        child.renderGroup = parent.renderGroup;
        child.billboardMode = parent.billboardMode;
        child.atlasCols = max(parent.atlasCols, 1u);
        child.atlasRows = max(parent.atlasRows, 1u);
        child.animFrameCount = max(parent.animFrameCount, 1u);
        child.animFps = parent.animFps;
        child.animFlags = parent.animFlags;
        child.animSpeed = max(parent.animSpeed, 0.01f);
        child.startFrame = parent.startFrame;

        float startZ = (parent.subEmitterStartSize.x + parent.subEmitterStartSize.y) * 0.5f;
        float endZ = (parent.subEmitterEndSize.x + parent.subEmitterEndSize.y) * 0.5f;
        child.startScale = float3(parent.subEmitterStartSize, startZ);
        child.endScale = float3(parent.subEmitterEndSize, endZ);
        child.scale = child.startScale;
        child.startColor = saturate(parent.subEmitterStartColor);
        child.endColor = saturate(parent.subEmitterEndColor);
        child.color = child.startColor;
        child.customFlags = GPU_PARTICLE_CUSTOM_DESC_OVERRIDE;
        if (parent.subEmitterAlphaFade != 0u) child.customFlags |= GPU_PARTICLE_CUSTOM_ALPHA_FADE;
        child.sizeCurveLut = 1.0f;
        child.gravity = parent.gravity;
        child.damping = parent.damping;
        child.rotation = GPURand1(seed + 41.13f) * 6.2831853f;
        child.rotationSpeed = parent.rotationSpeed;
        child.rotation3D = parent.rotation3D;
        child.angularVelocity3D = parent.angularVelocity3D;

        // 子ParticleではEvent/Collisionを無効化し、Sub Emitterが再帰増殖しないことを保証する。
        child.collisionShape = GPU_PARTICLE_COLLISION_NONE;
        child.eventMask = 0u;
        child.subEmitterEventMask = 0u;
        child.subEmitterCount = 0u;
        gParticles[childIndex] = child;
    }
}

bool ResolveAuthoredCollision(inout Particle p, out float3 collisionNormal, out bool shouldKill)
{
    collisionNormal = float3(0.0f, 1.0f, 0.0f);
    shouldKill = false;
    if ((p.customFlags & GPU_PARTICLE_CUSTOM_COLLISION) == 0u || p.collisionShape == GPU_PARTICLE_COLLISION_NONE)
        return false;

    float radius = max(p.collisionParticleRadius, 0.0f);
    bool collided = false;

    if (p.collisionShape == GPU_PARTICLE_COLLISION_PLANE)
    {
        collisionNormal = SafeEventDirection(p.collisionPlaneNormal, float3(0.0f, 1.0f, 0.0f));
        float signedDistance = dot(p.translate, collisionNormal) - p.collisionPlaneDistance;
        if (signedDistance < radius)
        {
            p.translate += collisionNormal * (radius - signedDistance);
            collided = true;
        }
    }
    else if (p.collisionShape == GPU_PARTICLE_COLLISION_SPHERE)
    {
        float3 delta = p.translate - p.collisionSphereCenter;
        float distanceToCenter = length(delta);
        float contactDistance = max(p.collisionSphereRadius, 0.0f) + radius;
        if (distanceToCenter < contactDistance)
        {
            collisionNormal = SafeEventDirection(delta, float3(0.0f, 1.0f, 0.0f));
            p.translate = p.collisionSphereCenter + collisionNormal * contactDistance;
            collided = true;
        }
    }

    if (!collided) return false;
    if (p.collisionResponse == GPU_PARTICLE_COLLISION_KILL)
    {
        shouldKill = true;
        return true;
    }

    float normalVelocity = dot(p.velocity, collisionNormal);
    if (p.collisionResponse == GPU_PARTICLE_COLLISION_SLIDE)
    {
        float3 tangentVelocity = p.velocity - collisionNormal * normalVelocity;
        p.velocity = tangentVelocity * (1.0f - saturate(p.collisionFriction)) + collisionNormal * max(normalVelocity, 0.0f);
    }
    else if (normalVelocity < 0.0f)
    {
        p.velocity -= collisionNormal * ((1.0f + saturate(p.collisionRestitution)) * normalVelocity);
        float postNormalVelocity = dot(p.velocity, collisionNormal);
        float3 tangentVelocity = p.velocity - collisionNormal * postNormalVelocity;
        p.velocity -= tangentVelocity * saturate(p.collisionFriction);
    }
    return true;
}

[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint particleIndex = DTid.x;
    if (particleIndex >= kMaxParticleCount) return;

    Particle p = gParticles[particleIndex];
    if (p.lifeTime <= 0.0f) return;

    float dt = gPerFrame.deltaTime;
    p.currentTime += dt;

    if (p.currentTime >= p.lifeTime)
    {
        SpawnSubEmitter(p, GPU_PARTICLE_EVENT_DEATH, p.translate, float3(0.0f, 1.0f, 0.0f), particleIndex);
        ReturnParticleToFreeList(particleIndex, p);
        return;
    }

    float t = saturate(p.currentTime / max(p.lifeTime, 1e-5f));
    if ((p.customFlags & GPU_PARTICLE_CUSTOM_DESC_OVERRIDE) != 0u)
    {
        float damp = max(0.0f, 1.0f - p.damping * dt);
        p.velocity *= damp;
        float3 acceleration = p.gravity;
        acceleration += EvaluateNoiseAcceleration(p);
        acceleration += EvaluateVortexAcceleration(p);
        acceleration += EvaluateAttractorAcceleration(p);
        p.velocity += acceleration * dt;
        p.translate += p.velocity * dt;

        float3 collisionNormal;
        bool collisionKill;
        bool collided = ResolveAuthoredCollision(p, collisionNormal, collisionKill);
        if (collided)
        {
            bool eventAlreadyLatched = (p.customFlags & GPU_PARTICLE_CUSTOM_COLLISION_LATCHED) != 0u;
            if (!eventAlreadyLatched)
            {
                SpawnSubEmitter(p, GPU_PARTICLE_EVENT_COLLISION, p.translate, collisionNormal, particleIndex);
                p.customFlags |= GPU_PARTICLE_CUSTOM_COLLISION_LATCHED;
            }
            if (collisionKill)
            {
                SpawnSubEmitter(p, GPU_PARTICLE_EVENT_DEATH, p.translate, collisionNormal, particleIndex);
                ReturnParticleToFreeList(particleIndex, p);
                return;
            }
        }
        else
        {
            p.customFlags &= ~GPU_PARTICLE_CUSTOM_COLLISION_LATCHED;
        }

        float sizeMultiplier = (p.customFlags & GPU_PARTICLE_CUSTOM_SIZE_CURVE) != 0u
            ? max(0.0f, SampleScalarLut(p.sizeCurveLut, t)) : 1.0f;
        p.scale = lerp(p.startScale, p.endScale, t) * sizeMultiplier;

        if ((p.customFlags & GPU_PARTICLE_CUSTOM_COLOR_GRADIENT) != 0u)
        {
            p.color = saturate(SampleColorGradient(p, t));
        }
        else
        {
            p.color = lerp(p.startColor, p.endColor, t);
            if ((p.customFlags & GPU_PARTICLE_CUSTOM_ALPHA_FADE) == 0u) p.color.a = p.startColor.a;
        }

        p.rotation += p.rotationSpeed * dt;
        p.rotation3D += p.angularVelocity3D * dt;
    }
    else
    {
        ParticleTypeParam param = GetParticleTypeParam(p.type);
        float damp = max(0.0f, 1.0f - param.drag * dt);
        p.velocity *= damp;
        p.velocity.y += param.accelY * dt;
        p.translate += p.velocity * dt;

        if (param.scaleGrow > 0.0f)
        {
            uint kind = GPUParticle_GetKind(p.billboardMode);
            if (kind == GPU_PARTICLE_KIND_SPRITE || kind == GPU_PARTICLE_KIND_RIBBON) p.scale.xy += param.scaleGrow * dt;
            else p.scale += param.scaleGrow * dt;
        }
        if (param.scaleShrink > 0.0f) p.scale *= max(0.0f, 1.0f - param.scaleShrink * dt);
        p.color.a = saturate(pow(1.0f - t, param.alphaPow) * param.baseAlpha);
    }

    gParticles[particleIndex] = p;
}
