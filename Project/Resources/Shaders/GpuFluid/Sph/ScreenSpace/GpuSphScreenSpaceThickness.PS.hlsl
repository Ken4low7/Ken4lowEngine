#include "GpuSphScreenSpaceCommon.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float2 localPosition : TEXCOORD0;
    float3 viewCenter : TEXCOORD1;
};

struct PSOutput
{
    float thickness : SV_Target0;
    float depth : SV_Depth;
};

PSOutput main(PSInput input)
{
    const float radiusSquared = dot(input.localPosition, input.localPosition);
    if (radiusSquared > 1.0f)
    {
        discard;
    }

    const float radius = GpuSphParticleRadius();
    const float sphereZ = sqrt(max(1.0f - radiusSquared, 0.0f));
    const float3 frontViewPosition = float3(
        input.viewCenter.xy + input.localPosition * radius,
        input.viewCenter.z - sphereZ * radius);
    if (frontViewPosition.z <= gRender.cameraParams.x)
    {
        discard;
    }

    const float4 clipPosition = mul(float4(frontViewPosition, 1.0f), gRender.projection);
    PSOutput output;
    output.thickness = 2.0f * sphereZ * radius;
    output.depth = saturate(clipPosition.z / max(clipPosition.w, 1.0e-6f));
    return output;
}
