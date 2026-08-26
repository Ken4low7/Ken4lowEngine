#include "GpuSphScreenSpaceCommon.hlsli"

StructuredBuffer<GpuSphParticle> gParticles : register(t0);

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 localPosition : TEXCOORD0;
    float3 viewCenter : TEXCOORD1;
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
    const float3 worldPosition = gParticles[instanceId].position;
    const float3 viewCenter = mul(float4(worldPosition, 1.0f), gRender.view).xyz;
    const float3 billboardViewPosition = viewCenter + float3(local * radius, 0.0f);

    output.position = mul(float4(billboardViewPosition, 1.0f), gRender.projection);
    output.localPosition = local;
    output.viewCenter = viewCenter;
    return output;
}
