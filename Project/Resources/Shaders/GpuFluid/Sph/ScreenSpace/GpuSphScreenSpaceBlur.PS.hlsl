#include "GpuSphScreenSpaceCommon.hlsli"

Texture2D<float> gDepthTexture : register(t0);
SamplerState gLinearClamp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float main(PSInput input) : SV_Target0
{
    const float centerDepth = gDepthTexture.SampleLevel(gLinearClamp, input.uv, 0.0f);
    if (!GpuSphHasFluidDepth(centerDepth))
    {
        return GpuSphFarClip();
    }

    const float2 texel = gRender.blurDirection.xy / GpuSphScreenSize();
    const float depthFalloff = max(gRender.cameraParams.w, 0.001f);
    float weightedDepth = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int offset = -4; offset <= 4; ++offset)
    {
        const float sampleDepth = gDepthTexture.SampleLevel(
            gLinearClamp,
            input.uv + texel * float(offset),
            0.0f);
        if (!GpuSphHasFluidDepth(sampleDepth))
        {
            continue;
        }

        const float spatialWeight = exp(-0.5f * float(offset * offset) / 4.0f);
        const float rangeWeight = exp(-abs(sampleDepth - centerDepth) * depthFalloff);
        const float weight = spatialWeight * rangeWeight;
        weightedDepth += sampleDepth * weight;
        weightSum += weight;
    }

    return weightSum > 1.0e-5f ? weightedDepth / weightSum : centerDepth;
}
