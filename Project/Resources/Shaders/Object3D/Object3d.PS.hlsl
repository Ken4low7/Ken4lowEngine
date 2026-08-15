#include "Object3d.hlsli"
#include "LightingCommon.hlsli"
#include "PbrDirectLighting.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct Material
{
    float4 color;
    float shininess;
    float pbrEnabled;
    float metallic;
    float normalScale;
    float4x4 uvTransform;
    float reflectionRate;
    float roughness;
    float usePointSampling;
    float occlusionStrength;
    float4 emissiveFactor;
    uint textureFlags;
    float reflectionSourceAvailable;
    float planarReflectionEnabled;
    float planarReflectionStrength;
};

struct Camera
{
    float3 worldPosition;
    float padding0;
};

struct LightInfo
{
    uint gLightCount;
    float padding0;
};

struct DissolveSetting
{
    float threshold;
    float edgeThickness;
    float4 edgeColor;
    float3 padding;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<LightInfo> gLightInfo : register(b2);
ConstantBuffer<DissolveSetting> gDissolveSetting : register(b3);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);
ConstantBuffer<LightingSettings> gLightingSettings : register(b5);
ConstantBuffer<ExtendedShadowParameter> gExtendedShadowParameter : register(b6);

Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
StructuredBuffer<PunctualLight> gPunctualLights : register(t2);
Texture2D<float4> gDissolveMaskTexture : register(t3);
Texture2D<float> gShadowMap : register(t4);
Texture2D<float4> gMetallicRoughnessTexture : register(t6);
Texture2D<float4> gNormalTexture : register(t7);
Texture2D<float4> gOcclusionTexture : register(t8);
Texture2D<float4> gEmissiveTexture : register(t9); // Planar鏡面Draw中だけReflection Textureとして再利用する。
Texture2DArray<float> gCsmShadowMaps : register(t10);
TextureCube<float> gPointShadowMap : register(t11);

SamplerState gPointSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);
SamplerState gLinearSampler : register(s2);

static const float kAlphaDiscardThreshold = 0.001f;

float ComputeFresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    // SRGB SRV は Sample 時点で線形化されるため、手動の pow による二重変換を避ける。
    // 低解像度テクスチャでは Point、通常モデルは Linear を使い分ける。
    float4 textureColor = (gMaterial.usePointSampling > 0.5f)
        ? gTexture.Sample(gPointSampler, transformedUV.xy)
        : gTexture.Sample(gLinearSampler, transformedUV.xy);

    float3 worldPosition = input.worldPosition;
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(gCamera.worldPosition - worldPosition);

    float spotShadowFactor = 1.0f;
    if (gShadowParameter.shadowMode == 2)
    {
        // SpotLightのDirectLightに掛けるshadowFactorをデバッグ表示にも再利用する。
        float3 dominantSpotDir = float3(0.0f, -1.0f, 0.0f);
        [loop]
        for (uint i = 0; i < gLightInfo.gLightCount; ++i)
        {
            if (gPunctualLights[i].lightType == 3)
            {
                dominantSpotDir = normalize(gPunctualLights[i].position - worldPosition);
                break;
            }
        }
        spotShadowFactor = CalculateShadow(worldPosition, normal, dominantSpotDir, gShadowParameter, gShadowMap, gShadowSampler);
    }
    else if (gShadowParameter.shadowMode == 4)
    {
        float3 directionalLightDir = float3(0.0f, 1.0f, 0.0f);
        [loop]
        for (uint directionalIndex = 0; directionalIndex < gLightInfo.gLightCount; ++directionalIndex)
        {
            if (gPunctualLights[directionalIndex].lightType == 1)
            {
                directionalLightDir = normalize(-gPunctualLights[directionalIndex].direction);
                break;
            }
        }
        spotShadowFactor = CalculateCsmShadow(worldPosition, normal, directionalLightDir, gExtendedShadowParameter, gCsmShadowMaps, gShadowSampler);
    }
    else if (gShadowParameter.shadowMode == 3 && gExtendedShadowParameter.shadowCasterLightIndex < gLightInfo.gLightCount)
    {
        PunctualLight pointCaster = gPunctualLights[gExtendedShadowParameter.shadowCasterLightIndex];
        float3 pointLightDir = normalize(pointCaster.position - worldPosition);
        spotShadowFactor = CalculatePointCubeShadow(worldPosition, normal, pointLightDir, gExtendedShadowParameter, gPointShadowMap, gShadowSampler);
    }

    if (gShadowParameter.shadowDebugMode == 1)
    {
        if (gShadowParameter.shadowMode == 3)
        {
            float3 cubeDirection = normalize(worldPosition - gExtendedShadowParameter.pointLightPositionAndFar.xyz);
            float cubeDepth = gPointShadowMap.SampleLevel(gLinearSampler, cubeDirection, 0.0f);
            output.color = float4(cubeDepth.xxx, 1.0f);
            return output;
        }
        float4x4 debugLightViewProjection = (gShadowParameter.shadowMode == 4)
            ? gExtendedShadowParameter.cascadeLightViewProjection[SelectShadowCascade(length(worldPosition - gExtendedShadowParameter.cameraPositionAndPointNear.xyz), gExtendedShadowParameter)]
            : gShadowParameter.lightViewProjection;
        float4 shadowPosition = mul(float4(worldPosition, 1.0f), debugLightViewProjection);
        float3 proj = shadowPosition.xyz / max(shadowPosition.w, 1e-5f);
        float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
        uint debugCascade = SelectShadowCascade(length(worldPosition - gExtendedShadowParameter.cameraPositionAndPointNear.xyz), gExtendedShadowParameter);
        float depth = (gShadowParameter.shadowMode == 4)
            ? gCsmShadowMaps.SampleLevel(gLinearSampler, float3(saturate(uv), (float)debugCascade), 0.0f)
            : gShadowMap.SampleLevel(gLinearSampler, saturate(uv), 0.0f);
        output.color = float4(depth.xxx, 1.0f);
        return output;
    }
    if (gShadowParameter.shadowDebugMode == 2)
    {
        output.color = float4(spotShadowFactor.xxx, 1.0f);
        return output;
    }

    float3 lighting = AccumulateLighting(gPunctualLights,
        gLightInfo.gLightCount,
        worldPosition,
        normal,
        viewDir,
        gShadowParameter,
        gMaterial.shininess,
        gShadowMap,
        gExtendedShadowParameter,
        gCsmShadowMaps,
        gPointShadowMap,
        gShadowSampler,
        gLightingSettings);

    float3 baseColor = gMaterial.color.rgb * textureColor.rgb * input.instanceColor.rgb;

    // Dissolve
    float maskValue = gDissolveMaskTexture.Sample(gLinearSampler, input.texcoord).r;
    float edge = smoothstep(
        gDissolveSetting.threshold,
        gDissolveSetting.threshold + gDissolveSetting.edgeThickness,
        maskValue);

    float4 edgeColor = gDissolveSetting.edgeColor * (1.0f - edge);
    float dissolveBlend = 1.0f - step(maskValue, gDissolveSetting.threshold);

    float3 shadedColor = 0.0.xxx;
    if (gMaterial.pbrEnabled > 0.5f)
    {
        // PBRはMaterial単位で明示ONの時だけ使い、Legacy描画の初期見た目を維持する。
        float2 metallicRoughness = ResolveMetallicRoughnessFallback(gMaterial.metallic, gMaterial.roughness);
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_METALLIC_ROUGHNESS) != 0)
        {
            float4 metallicRoughnessSample = gMetallicRoughnessTexture.Sample(gLinearSampler, transformedUV.xy);
            metallicRoughness.x *= metallicRoughnessSample.b; // glTF規約: B=Metallic
            metallicRoughness.y *= metallicRoughnessSample.g; // glTF規約: G=Roughness
        }

        float3 pbrNormal = ResolvePbrNormalFallback(normal, gMaterial.normalScale);
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_NORMAL) != 0)
        {
            pbrNormal = ResolvePbrNormalMap(
                normal,
                worldPosition,
                transformedUV.xy,
                gNormalTexture.Sample(gLinearSampler, transformedUV.xy).xyz,
                gMaterial.normalScale);
        }

        float occlusion = 1.0f;
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_OCCLUSION) != 0)
        {
            float sampledOcclusion = gOcclusionTexture.Sample(gLinearSampler, transformedUV.xy).r;
            occlusion = lerp(1.0f, sampledOcclusion, saturate(gMaterial.occlusionStrength));
        }

        float3 emissiveSample = 1.0.xxx;
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_EMISSIVE) != 0 && gMaterial.planarReflectionEnabled <= 0.5f)
        {
            emissiveSample = gEmissiveTexture.Sample(gLinearSampler, transformedUV.xy).rgb;
        }

        PbrSurface surface;
        surface.baseColor = lerp(baseColor, edgeColor.rgb, dissolveBlend);
        surface.metallic = metallicRoughness.x;
        surface.roughness = metallicRoughness.y;
        surface.occlusion = occlusion;
        surface.emissive = gMaterial.emissiveFactor.rgb * emissiveSample;
        surface.normal = pbrNormal;
        surface.viewDir = viewDir;

        float3 directPbr = DirectLightingPBR(gPunctualLights, gLightInfo.gLightCount, worldPosition, surface, gShadowParameter, gShadowMap, gExtendedShadowParameter, gCsmShadowMaps, gPointShadowMap, gShadowSampler);
        float3 ibl = 0.0.xxx;
        if (gMaterial.reflectionSourceAvailable > 0.5f)
        {
            ibl = EvaluatePbrIBLFallback(surface, gEnvironmentTexture, gLinearSampler, gLightingSettings);
        }
        shadedColor = directPbr + ibl + surface.emissive;
    }
    else
    {
        // Legacyは反射率0または反射源なしを明確なEnvironment OFFとして扱う。
        shadedColor = lerp(baseColor, edgeColor.rgb, dissolveBlend);
        shadedColor *= lighting;
        const float reflectionRate = saturate(gMaterial.reflectionRate);
        if (reflectionRate > 0.0f && gMaterial.reflectionSourceAvailable > 0.5f)
        {
            float3 reflectionDir = reflect(-viewDir, normal);
            uint environmentWidth = 0;
            uint environmentHeight = 0;
            uint environmentMipLevels = 1;
            gEnvironmentTexture.GetDimensions(0, environmentWidth, environmentHeight, environmentMipLevels);
            float maxMipLevel = (environmentMipLevels > 0) ? float(environmentMipLevels - 1) : 0.0f;
            float reflectionMipLevel = saturate(gMaterial.roughness) * maxMipLevel;
            float3 environmentColor = gEnvironmentTexture.SampleLevel(gLinearSampler, reflectionDir, reflectionMipLevel).rgb;
            float fresnel = ComputeFresnelSchlick(saturate(dot(normal, viewDir)), 0.02f);
            float envBlend = saturate(reflectionRate + fresnel * reflectionRate * (1.0f - reflectionRate));
            shadedColor = lerp(shadedColor, environmentColor, envBlend); // 反射率1.0は環境反射確認に使える強さまで反映する。
        }
        shadedColor += gMaterial.emissiveFactor.rgb;
    }

    // Fog/ToneMap/Contrastを通常Scene色へ先に適用し、反射RTを二重ToneMapしない。
    shadedColor = ApplyFog(shadedColor, worldPosition, gCamera.worldPosition, gLightingSettings);
    shadedColor = ApplySimpleToneMapping(shadedColor, gLightingSettings);
    shadedColor = ApplyContrast(shadedColor, gLightingSettings);

    if (gMaterial.planarReflectionEnabled > 0.5f)
    {
        uint planarWidth = 1;
        uint planarHeight = 1;
        gEmissiveTexture.GetDimensions(planarWidth, planarHeight);
        float2 planarSize = max(float2((float)planarWidth, (float)planarHeight), float2(1.0f, 1.0f));
        float2 planarUv = input.position.xy / planarSize;
        planarUv.x = 1.0f - planarUv.x; // 通常LookAtが保持する右手系のX反転を戻し、鏡面Pixelと反射Camera画像を一致させる。
        const float2 uvMin = float2(0.0f, 0.0f);
        const float2 uvMax = float2(1.0f, 1.0f);
        if (all(planarUv >= uvMin) && all(planarUv <= uvMax))
        {
            float3 planarColor = gEmissiveTexture.SampleLevel(gLinearSampler, saturate(planarUv), 0.0f).rgb;
            shadedColor = lerp(shadedColor, planarColor, saturate(gMaterial.planarReflectionStrength));
        }
    }

    output.color.rgb = shadedColor;
    output.color.a = gMaterial.color.a * textureColor.a * input.instanceColor.a;

    if (output.color.a < kAlphaDiscardThreshold)
    {
        discard;
    }
    return output;
}