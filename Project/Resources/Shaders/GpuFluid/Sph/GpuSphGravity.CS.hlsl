#include "GpuSphCommon.hlsli"

[numthreads(128, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    const uint index = dispatchThreadId.x;
    if (!GpuSphIsActiveParticle(index))
    {
        return;
    }

    GpuSphParticle particle = gParticles[index];
    particle.velocity += gSph.gravity * gSph.deltaTime;

    if (gSph.oceanCouplingEnabled != 0u)
    {
        const float3 oceanNormal = normalize(gSph.oceanSurfaceNormal);
        const float signedDistance = dot(particle.position - gSph.oceanSurfacePoint, oceanNormal);
        const float blendBand = max(gSph.oceanBlendBand, 1.0e-3f);
        const float influence = saturate(1.0f - abs(signedDistance) / blendBand);
        const float submergedInfluence = signedDistance <= 0.0f ? 1.0f : influence;

        const float velocityAlpha = saturate(gSph.oceanVelocityCoupling * submergedInfluence * gSph.deltaTime);
        particle.velocity = lerp(particle.velocity, gSph.oceanSurfaceVelocity, velocityAlpha);

        const float attraction = clamp(
            -signedDistance * gSph.oceanSurfaceAttraction,
            -gSph.oceanMaxCorrection,
            gSph.oceanMaxCorrection);
        particle.velocity += oceanNormal * attraction * influence * gSph.deltaTime;
    }

    gParticles[index] = particle; // W10は既存Gravity Pass内でOcean連成し、追加PSOなしでPrimary SPHへ波面運動を渡す。
}
