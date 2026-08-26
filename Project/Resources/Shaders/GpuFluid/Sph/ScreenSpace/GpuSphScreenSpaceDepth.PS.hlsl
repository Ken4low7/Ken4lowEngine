#include "GpuSphScreenSpaceCommon.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float2 localPosition : TEXCOORD0;
    float3 viewCenter : TEXCOORD1;
    float2 viewOffset : TEXCOORD2;
};

struct PSOutput
{
    float linearDepth : SV_Target0;
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
    // W8.5ではXYを異方性Splatの実オフセット、Zを基準半径の球面として再構成する。
    const float3 viewPosition = float3(
        input.viewCenter.xy + input.viewOffset,
        input.viewCenter.z - sphereZ * radius);
    if (viewPosition.z <= gRender.cameraParams.x)
    {
        discard;
    }

    const float4 clipPosition = mul(float4(viewPosition, 1.0f), gRender.projection);
    PSOutput output;
    output.linearDepth = viewPosition.z;
    output.depth = saturate(clipPosition.z / max(clipPosition.w, 1.0e-6f));
    return output;
}
