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

float3 SafeNormalFromDepth(float2 uv, float centerDepth)
{
    const float2 texel = 1.0f / GpuSphScreenSize();
    float rightDepth = gFluidDepth.SampleLevel(gLinearClamp, uv + float2(texel.x, 0.0f), 0.0f);
    float upDepth = gFluidDepth.SampleLevel(gLinearClamp, uv - float2(0.0f, texel.y), 0.0f);
    if (!GpuSphHasFluidDepth(rightDepth)) rightDepth = centerDepth;
    if (!GpuSphHasFluidDepth(upDepth)) upDepth = centerDepth;

    const float3 centerPosition = GpuSphReconstructViewPosition(uv, centerDepth);
    const float3 rightPosition = GpuSphReconstructViewPosition(uv + float2(texel.x, 0.0f), rightDepth);
    const float3 upPosition = GpuSphReconstructViewPosition(uv - float2(0.0f, texel.y), upDepth);
    float3 normalValue = normalize(cross(upPosition - centerPosition, rightPosition - centerPosition));
    const float3 viewDirection = normalize(-centerPosition);
    if (dot(normalValue, viewDirection) < 0.0f)
    {
        normalValue = -normalValue;
    }
    return normalValue;
}

float4 main(PSInput input) : SV_Target0
{
    const float depthValue = gFluidDepth.SampleLevel(gLinearClamp, input.uv, 0.0f);
    if (!GpuSphHasFluidDepth(depthValue))
    {
        discard;
    }

    const float thicknessValue = max(gFluidThickness.SampleLevel(gLinearClamp, input.uv, 0.0f), 0.0f);
    const float3 viewPosition = GpuSphReconstructViewPosition(input.uv, depthValue);
    const float3 normalValue = SafeNormalFromDepth(input.uv, depthValue);
    const float3 viewDirection = normalize(-viewPosition);

    const float thicknessScale = max(gRender.opticalParams.w, 0.0f);
    const float normalizedThickness = saturate(thicknessValue * thicknessScale);
    const float absorption = 1.0f - exp(-thicknessValue * max(gRender.opticalParams.x, 0.0f));
    const float refractionStrength = gRender.opticalParams.y;
    const float2 refractedUv = saturate(input.uv + normalValue.xy * refractionStrength * normalizedThickness);
    const float3 refractedScene = gSceneColor.SampleLevel(gLinearClamp, refractedUv, 0.0f).rgb;

    const float3 waterTint = lerp(gRender.shallowColor.rgb, gRender.deepColor.rgb, normalizedThickness);
    float3 color = lerp(refractedScene, waterTint, saturate(absorption * 0.82f));

    const float fresnel = pow(
        1.0f - saturate(dot(normalValue, viewDirection)),
        max(gRender.opticalParams.z, 1.0f));
    const float3 skyHighlight = float3(0.72f, 0.90f, 1.0f);
    color = lerp(color, skyHighlight, saturate(fresnel * 0.42f));

    // Screen Space Normalから簡易Specularを作り、粒子感ではなく連続した液体表面として見せる。
    const float3 lightDirection = normalize(float3(-0.35f, 0.75f, -0.55f));
    const float3 halfVector = normalize(lightDirection + viewDirection);
    const float specular = pow(saturate(dot(normalValue, halfVector)), 96.0f);
    color += specular * float3(0.9f, 0.98f, 1.0f) * 0.6f;

    return float4(color, 1.0f);
}
