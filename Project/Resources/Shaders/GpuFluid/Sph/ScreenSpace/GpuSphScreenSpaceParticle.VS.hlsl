#include "GpuSphScreenSpaceCommon.hlsli"

StructuredBuffer<GpuSphParticle> gParticles : register(t0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 localPosition : TEXCOORD0;
    float3 viewCenter : TEXCOORD1;
    float2 viewOffset : TEXCOORD2;
};

VSOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    const float2 corners[6] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  1.0f),
        float2( 1.0f,  1.0f),
        float2(-1.0f, -1.0f),
        float2( 1.0f,  1.0f),
        float2( 1.0f, -1.0f)
    };

    VSOutput output;
    const float2 local = corners[vertexId % 6u];
    const float radius = GpuSphParticleRadius();
    const GpuSphParticle particle = gParticles[instanceId];
    const float3 viewCenter = mul(float4(particle.position, 1.0f), gRender.view).xyz;

    // W8.5: 流れている粒子だけ速度方向へ伸ばし、丸い数珠状の輪郭を連続した流体表面へ近づける。
    const float2 viewVelocity = mul(float4(particle.velocity, 0.0f), gRender.view).xy;
    const float viewSpeed = length(viewVelocity);
    const float2 majorAxis = viewSpeed > 1.0e-4f ? viewVelocity / viewSpeed : float2(1.0f, 0.0f);
    const float2 minorAxis = float2(-majorAxis.y, majorAxis.x);
    const float targetStretch = min(1.0f + viewSpeed * 0.22f, 1.85f);
    const float stretch = lerp(1.0f, targetStretch, 0.45f);
    const float transverseScale = rsqrt(max(stretch, 1.0f));
    const float2 viewOffset =
        majorAxis * (local.x * radius * stretch) +
        minorAxis * (local.y * radius * transverseScale);

    const float3 billboardViewPosition = viewCenter + float3(viewOffset, 0.0f);
    output.position = mul(float4(billboardViewPosition, 1.0f), gRender.projection);
    output.localPosition = local;
    output.viewCenter = viewCenter;
    output.viewOffset = viewOffset;
    return output;
}
