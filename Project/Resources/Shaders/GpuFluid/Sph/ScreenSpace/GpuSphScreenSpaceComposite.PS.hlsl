#include "GpuSphScreenSpaceCommon.hlsli"

Texture2D<float> gFluidDepth : register(t0);
Texture2D<float> gFluidThickness : register(t1);
Texture2D<float4> gSceneColor : register(t2);
SamplerState gLinearClamp : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float SampleSafeDepth(float2 uv, float fallbackDepth)
{
    const float value = gFluidDepth.SampleLevel(gLinearClamp, saturate(uv), 0.0f);
    return GpuSphHasFluidDepth(value) ? value : fallbackDepth;
}

float3 SafeNormalFromDepth(float2 uv, float centerDepth)
{
    const float2 texel = 1.0f / GpuSphScreenSize();
    const float leftDepth = SampleSafeDepth(uv - float2(texel.x, 0.0f), centerDepth);
    const float rightDepth = SampleSafeDepth(uv + float2(texel.x, 0.0f), centerDepth);
    const float upDepth = SampleSafeDepth(uv - float2(0.0f, texel.y), centerDepth);
    const float downDepth = SampleSafeDepth(uv + float2(0.0f, texel.y), centerDepth);

    const float3 centerPosition = GpuSphReconstructViewPosition(uv, centerDepth);
    const float3 leftPosition = GpuSphReconstructViewPosition(uv - float2(texel.x, 0.0f), leftDepth);
    const float3 rightPosition = GpuSphReconstructViewPosition(uv + float2(texel.x, 0.0f), rightDepth);
    const float3 upPosition = GpuSphReconstructViewPosition(uv - float2(0.0f, texel.y), upDepth);
    const float3 downPosition = GpuSphReconstructViewPosition(uv + float2(0.0f, texel.y), downDepth);

    const float3 dx = abs(rightDepth - centerDepth) <= abs(centerDepth - leftDepth)
        ? rightPosition - centerPosition
        : centerPosition - leftPosition;
    const float3 dy = abs(downDepth - centerDepth) <= abs(centerDepth - upDepth)
        ? downPosition - centerPosition
        : centerPosition - upPosition;

    float3 normalValue = normalize(cross(dy, dx));
    const float3 viewDirection = normalize(-centerPosition);
    if (dot(normalValue, viewDirection) < 0.0f)
    {
        normalValue = -normalValue;
    }
    return normalValue;
}

float ComputeFoam(float2 uv, float centerDepth, float thicknessValue)
{
    const float2 texel = 1.0f / GpuSphScreenSize();
    const float leftDepth = SampleSafeDepth(uv - float2(texel.x, 0.0f), centerDepth);
    const float rightDepth = SampleSafeDepth(uv + float2(texel.x, 0.0f), centerDepth);
    const float upDepth = SampleSafeDepth(uv - float2(0.0f, texel.y), centerDepth);
    const float downDepth = SampleSafeDepth(uv + float2(0.0f, texel.y), centerDepth);
    const float depthCurvature = abs(leftDepth + rightDepth + upDepth + downDepth - centerDepth * 4.0f) /
        max(GpuSphParticleRadius(), 0.001f);

    const float leftThickness = max(gFluidThickness.SampleLevel(gLinearClamp, uv - float2(texel.x, 0.0f), 0.0f), 0.0f);
    const float rightThickness = max(gFluidThickness.SampleLevel(gLinearClamp, uv + float2(texel.x, 0.0f), 0.0f), 0.0f);
    const float upThickness = max(gFluidThickness.SampleLevel(gLinearClamp, uv - float2(0.0f, texel.y), 0.0f), 0.0f);
    const float downThickness = max(gFluidThickness.SampleLevel(gLinearClamp, uv + float2(0.0f, texel.y), 0.0f), 0.0f);
    const float thicknessGradient = length(float2(rightThickness - leftThickness, downThickness - upThickness));

    const float surfaceSignal = depthCurvature * 0.55f + thicknessGradient * 2.25f;
    const float thinSurface = 1.0f - saturate(thicknessValue * max(gRender.opticalParams.w, 0.0f) * 0.65f);
    return smoothstep(0.32f, 0.72f, surfaceSignal) * thinSurface;
}

float4 main(PSInput input) : SV_Target0
{
    const float depthValue = gFluidDepth.SampleLevel(gLinearClamp, input.uv, 0.0f);
    if (!GpuSphHasFluidDepth(depthValue))
    {
        discard;
    }

    const float3 sceneBase = gSceneColor.SampleLevel(gLinearClamp, input.uv, 0.0f).rgb;
    const float thicknessValue = max(gFluidThickness.SampleLevel(gLinearClamp, input.uv, 0.0f), 0.0f);
    const float3 viewPosition = GpuSphReconstructViewPosition(input.uv, depthValue);
    const float3 normalValue = SafeNormalFromDepth(input.uv, depthValue);
    const float3 viewDirection = normalize(-viewPosition);

    const float thicknessScale = max(gRender.opticalParams.w, 0.0f);
    const float localLiquidBlend = saturate(thicknessScale / 5.0f);
    const float normalizedThickness = saturate(thicknessValue * thicknessScale);
    const float refractionStrength = gRender.opticalParams.y;
    const float2 refractedUv = saturate(input.uv + normalValue.xy * refractionStrength * (0.35f + normalizedThickness * 0.65f));
    const float3 refractedScene = gSceneColor.SampleLevel(gLinearClamp, refractedUv, 0.0f).rgb;

    const float absorption = max(gRender.opticalParams.x, 0.0f);
    const float3 extinction = max((1.0f - saturate(gRender.deepColor.rgb)) * 0.42f, float3(0.08f, 0.035f, 0.012f));
    const float3 transmittance = exp(-extinction * absorption * thicknessValue);
    const float3 waterTint = lerp(gRender.shallowColor.rgb, gRender.deepColor.rgb, normalizedThickness);
    float3 color = refractedScene * transmittance + waterTint * (1.0f - transmittance);

    const float fresnel = pow(1.0f - saturate(dot(normalValue, viewDirection)), max(gRender.opticalParams.z, 1.0f));
    const float3 environmentReflection = lerp(float3(0.32f, 0.48f, 0.62f), float3(0.78f, 0.92f, 1.0f), saturate(normalValue.y * 0.5f + 0.5f));
    color = lerp(color, environmentReflection, saturate(fresnel * 0.48f));

    const float3 lightDirection = normalize(float3(-0.35f, 0.75f, -0.55f));
    const float3 halfVector = normalize(lightDirection + viewDirection);
    const float specular = pow(saturate(dot(normalValue, halfVector)), 112.0f);
    color += specular * float3(0.92f, 0.98f, 1.0f) * 0.68f;

    const float foam = ComputeFoam(input.uv, depthValue, thicknessValue);
    color = lerp(color, float3(0.86f, 0.94f, 1.0f), saturate(foam * 0.24f));

    color = lerp(sceneBase, color, localLiquidBlend); // W10のBlend BandではSSFRを既描画Oceanへ戻し、境界のハードな切替を消す。
    return float4(color, 1.0f);
}
