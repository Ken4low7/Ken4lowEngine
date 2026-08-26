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

    // W8.5: 13tap Bilateralで粒子球の継ぎ目を消しつつ、別物体とのDepth境界は保持する。
    [unroll]
    for (int offset = -6; offset <= 6; ++offset)
    {
        const float sampleDepth = gDepthTexture.SampleLevel(
            gLinearClamp,
            input.uv + texel * float(offset),
            0.0f);
        if (!GpuSphHasFluidDepth(sampleDepth))
        {
            continue;
        }

        const float spatialWeight = exp(-0.5f * float(offset * offset) / 7.5f);
        const float rangeWeight = exp(-abs(sampleDepth - centerDepth) * depthFalloff);
        const float weight = spatialWeight * rangeWeight;
        weightedDepth += sampleDepth * weight;
        weightSum += weight;
    }

    const float bilateralDepth = weightSum > 1.0e-5f ? weightedDepth / weightSum : centerDepth;

    const float neighborA = gDepthTexture.SampleLevel(gLinearClamp, input.uv - texel, 0.0f);
    const float neighborB = gDepthTexture.SampleLevel(gLinearClamp, input.uv + texel, 0.0f);
    float curvatureTarget = bilateralDepth;
    float curvatureWeight = 1.0f;

    if (GpuSphHasFluidDepth(neighborA))
    {
        const float edgeWeight = exp(-abs(neighborA - centerDepth) * depthFalloff);
        curvatureTarget += neighborA * edgeWeight;
        curvatureWeight += edgeWeight;
    }
    if (GpuSphHasFluidDepth(neighborB))
    {
        const float edgeWeight = exp(-abs(neighborB - centerDepth) * depthFalloff);
        curvatureTarget += neighborB * edgeWeight;
        curvatureWeight += edgeWeight;
    }

    curvatureTarget /= max(curvatureWeight, 1.0e-5f);
    const float maxRelaxation = max(GpuSphParticleRadius() * 0.12f, 0.0001f);
    const float curvatureDelta = clamp(curvatureTarget - bilateralDepth, -maxRelaxation, maxRelaxation);
    return bilateralDepth + curvatureDelta * 0.32f;
}
