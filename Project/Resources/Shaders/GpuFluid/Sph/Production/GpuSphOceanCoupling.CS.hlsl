struct GpuSphParticle
{
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 predictedPosition;
    float padding;
};

cbuffer OceanCouplingCB : register(b0)
{
    uint activeParticleCount;
    float deltaTime;
    float blendBand;
    float velocityCoupling;
    float3 surfacePoint;
    float surfaceAttraction;
    float3 surfaceNormal;
    float maxCorrection;
    float3 surfaceVelocity;
    float padding0;
};

RWStructuredBuffer<GpuSphParticle> particles : register(u0);

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (index >= activeParticleCount)
    {
        return;
    }

    GpuSphParticle particle = particles[index];
    const float3 normalValue = normalize(surfaceNormal);
    const float signedDistance = dot(particle.position - surfacePoint, normalValue);
    const float safeBlendBand = max(blendBand, 1.0e-3f);
    const float edgeInfluence = saturate(1.0f - abs(signedDistance) / safeBlendBand);
    const float flowInfluence = signedDistance <= 0.0f ? 1.0f : edgeInfluence;

    const float flowAlpha = saturate(velocityCoupling * flowInfluence * deltaTime);
    particle.velocity = lerp(particle.velocity, surfaceVelocity, flowAlpha);

    const float correction = clamp(
        -signedDistance * surfaceAttraction,
        -maxCorrection,
        maxCorrection);
    particle.velocity += normalValue * correction * edgeInfluence * deltaTime;

    particles[index] = particle; // W10 Ocean→SPHは波面近傍だけをBlendし、遠方のLocal Liquid挙動を保持する。
}
