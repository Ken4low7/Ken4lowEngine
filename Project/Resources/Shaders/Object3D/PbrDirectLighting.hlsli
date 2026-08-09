#ifndef PBR_DIRECT_LIGHTING_HLSLI
#define PBR_DIRECT_LIGHTING_HLSLI

// PBR Direct Lightingの最小関数群。
// IBLは環境マップ/BRDF LUT/roughness mip設計が別途必要なため、Direct Lightとは別関数に分ける。
// Legacy MaterialとPBR Materialは既存モデル互換を守るため共存させ、MaterialのpbrEnabledがONの時だけ使う。
// 今回Deferred / Forward+ / SSAO / SSR / GIへ進まないのは、RootSignatureやRenderTarget構成の変更を避けて既存Forward描画を守るため。

static const float kPbrPI = 3.14159265f;
static const uint MATERIAL_TEXTURE_METALLIC_ROUGHNESS = 1u << 0;
static const uint MATERIAL_TEXTURE_NORMAL = 1u << 1;
static const uint MATERIAL_TEXTURE_OCCLUSION = 1u << 2;
static const uint MATERIAL_TEXTURE_EMISSIVE = 1u << 3;

struct PbrSurface
{
    float3 baseColor;
    float metallic;
    float roughness;
    float occlusion;
    float3 emissive;
    float3 normal;
    float3 viewDir;
};

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0.xxx - f0) * pow(1.0f - saturate(cosTheta), 5.0f);
}

float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
    float a = max(roughness * roughness, 0.001f);
    float a2 = a * a;
    float NdotH = saturate(dot(normal, halfVector));
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / max(kPbrPI * denom * denom, 1e-5f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 1e-5f);
}

float GeometrySmith(float3 normal, float3 viewDir, float3 lightDir, float roughness)
{
    float NdotV = saturate(dot(normal, viewDir));
    float NdotL = saturate(dot(normal, lightDir));
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 CookTorranceBRDF(PbrSurface surface, float3 lightDir, float3 lightColor)
{
    float3 normal = normalize(surface.normal);
    float3 viewDir = normalize(surface.viewDir);
    float3 halfVector = normalize(viewDir + lightDir);
    float roughness = clamp(surface.roughness, 0.04f, 1.0f);
    float metallic = saturate(surface.metallic);
    float NdotL = saturate(dot(normal, lightDir));
    float NdotV = saturate(dot(normal, viewDir));

    float3 f0 = lerp(0.04.xxx, surface.baseColor, metallic);
    float NDF = DistributionGGX(normal, halfVector, roughness);
    float G = GeometrySmith(normal, viewDir, lightDir, roughness);
    float3 F = FresnelSchlick(saturate(dot(halfVector, viewDir)), f0);

    float3 specular = (NDF * G * F) / max(4.0f * NdotV * NdotL, 1e-5f);
    float3 kD = (1.0.xxx - F) * (1.0f - metallic);
    float3 diffuse = kD * surface.baseColor / kPbrPI;

    return (diffuse + specular) * lightColor * NdotL;
}

float3 EvaluatePbrDirectLight(PbrSurface surface, float3 lightDir, float3 lightColor)
{
    return CookTorranceBRDF(surface, lightDir, lightColor);
}

float3 ResolvePbrNormalFallback(float3 vertexNormal, float normalScale)
{
    // Tangent未整備またはNormalMap未設定の場合は既存頂点法線へfallbackし、既存モデルが破綻しないことを優先する。
    return normalize(lerp(vertexNormal, vertexNormal, saturate(normalScale)));
}

float3 ResolvePbrNormalMap(
    float3 vertexNormal,
    float3 worldPosition,
    float2 texcoord,
    float3 encodedNormal,
    float normalScale)
{
    float3 normal = normalize(vertexNormal);
    float3 dpdx = ddx(worldPosition);
    float3 dpdy = ddy(worldPosition);
    float2 duvdx = ddx(texcoord);
    float2 duvdy = ddy(texcoord);
    float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;
    if (abs(determinant) < 1e-6f)
    {
        return ResolvePbrNormalFallback(normal, normalScale); // UVが退化した面は頂点法線へ安全に戻す。
    }

    float3 tangent = normalize((dpdx * duvdy.y - dpdy * duvdx.y) / determinant);
    tangent = normalize(tangent - normal * dot(normal, tangent));
    float3 bitangent = normalize(cross(normal, tangent)) * (determinant < 0.0f ? -1.0f : 1.0f);
    float3 tangentNormal = encodedNormal * 2.0f - 1.0f;
    tangentNormal.xy *= max(normalScale, 0.0f);
    tangentNormal = normalize(tangentNormal);
    return normalize(tangent * tangentNormal.x + bitangent * tangentNormal.y + normal * tangentNormal.z);
}

float2 ResolveMetallicRoughnessFallback(float metallicFactor, float roughnessFactor)
{
    // Metallic/Roughness Texture未接続時はMaterialCBDataの定数値を使い、glTF完全接続前でもPBRを黒化させない。
    return float2(saturate(metallicFactor), clamp(roughnessFactor, 0.04f, 1.0f));
}

float3 EvaluatePbrIBLFallback(PbrSurface surface, TextureCube<float4> environmentTexture, SamplerState samplerState, LightingSettings lightingSettings)
{
    if (lightingSettings.enableIBL == 0)
    {
        return 0.0.xxx;
    }

    // irradiance/prefilteredEnv/brdfLUT未実装時は、既存EnvironmentTextureとAmbientから安全な簡易IBLだけを作る。
    float3 normal = normalize(surface.normal);
    float3 viewDir = normalize(surface.viewDir);
    float3 reflectionDir = reflect(-viewDir, normal);
    float3 envSpecular = environmentTexture.Sample(samplerState, reflectionDir).rgb;
    float3 envDiffuse = lightingSettings.ambientColor.rgb * lightingSettings.ambientColor.a;
    float roughnessMask = 1.0f - clamp(surface.roughness, 0.04f, 1.0f);

    float3 diffuse = envDiffuse * surface.baseColor * lightingSettings.iblDiffuseStrength * surface.occlusion;
    float3 specular = envSpecular * roughnessMask * lightingSettings.iblSpecularStrength;
    return diffuse + specular;
}

float3 DirectLightingPBR(
    StructuredBuffer<PunctualLight> punctualLights,
    uint lightCount,
    float3 worldPosition,
    PbrSurface surface,
    ShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    ExtendedShadowParameter extendedShadowParam,
    Texture2DArray<float> csmShadowMaps,
    TextureCube<float> pointShadowMap,
    SamplerComparisonState shadowSampler)
{
    float3 result = 0.0.xxx;

    [loop]
    for (uint i = 0; i < lightCount; ++i)
    {
        PunctualLight light = punctualLights[i];
        if (light.enabled == 0 || light.lightType == LIGHT_TYPE_NONE)
        {
            continue;
        }

        float3 lightDir = 0.0.xxx;
        float attenuation = 1.0f;
        float shadow = 1.0f;

        if (light.lightType == LIGHT_TYPE_DIRECTIONAL)
        {
            lightDir = normalize(-light.direction);
            if (i == extendedShadowParam.shadowCasterLightIndex)
            {
                if (extendedShadowParam.shadowTechnique == SHADOW_TECHNIQUE_CSM)
                {
                    shadow = CalculateCsmShadow(worldPosition, surface.normal, lightDir, extendedShadowParam, csmShadowMaps, shadowSampler);
                }
                else if (extendedShadowParam.shadowTechnique == SHADOW_TECHNIQUE_DIRECTIONAL)
                {
                    shadow = CalculateDirectionalShadow(worldPosition, surface.normal, lightDir, extendedShadowParam, shadowMap, shadowSampler);
                }
            }
        }
        else if (light.lightType == LIGHT_TYPE_POINT)
        {
            float3 toL = light.position - worldPosition;
            float d = length(toL);
            lightDir = toL / max(d, kMinLightLength);
            float range = max(light.radius, kMinRange);
            attenuation = pow(saturate(1.0f - d / range), max(light.decay, kMinRange)) * step(d, range);
            if (extendedShadowParam.shadowTechnique == SHADOW_TECHNIQUE_POINT_CUBE && i == extendedShadowParam.shadowCasterLightIndex)
            {
                shadow = CalculatePointCubeShadow(worldPosition, surface.normal, lightDir, extendedShadowParam, pointShadowMap, shadowSampler);
            }
        }
        else if (light.lightType == LIGHT_TYPE_SPOT)
        {
            float3 toL = light.position - worldPosition;
            float d = length(toL);
            lightDir = toL / max(d, kMinLightLength);
            float range = max(light.distance, kMinRange);
            float rangeMask = step(d, range);
            attenuation = pow(saturate(1.0f - d / range), max(light.decay, kMinRange)) * rangeMask;
            float3 dir = normalize(light.direction);
            float ct = dot(-dir, lightDir);
            attenuation *= smoothstep(light.cosAngle, light.cosFalloffStart, ct) * step(light.cosAngle, ct);
            if (extendedShadowParam.shadowTechnique == SHADOW_TECHNIQUE_SPOT_LINEAR && i == extendedShadowParam.shadowCasterLightIndex)
            {
                shadow = CalculateSpotLinearShadow(worldPosition, surface.normal, lightDir, extendedShadowParam, shadowMap, shadowSampler);
            }
        }
        else
        {
            // AreaLightの本格PBRは別途LTC等が必要なため、今回はPoint近似のLegacy側を残しPBRでは未接続にする。
            continue;
        }

        float3 lightColor = light.color.rgb * light.intensity * attenuation * shadow;
        result += EvaluatePbrDirectLight(surface, lightDir, lightColor);
    }

    return result;
}

#endif