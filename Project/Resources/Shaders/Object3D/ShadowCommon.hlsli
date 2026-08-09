#ifndef SHADOW_COMMON_HLSLI
#define SHADOW_COMMON_HLSLI

static const float kEpsilon = 1e-5f;
static const float kDefaultShadowStrength = 0.60f;
static const int kPCFRadius = 1;
static const float kMinimumReceiverDepthBias = 0.00025f;
static const float kReceiverSlopeBiasScale = 1.25f;
static const float kMaximumReceiverSlope = 4.0f;

static const uint SHADOW_TECHNIQUE_OFF = 0;
static const uint SHADOW_TECHNIQUE_DIRECTIONAL = 1;
static const uint SHADOW_TECHNIQUE_SPOT_LINEAR = 2;
static const uint SHADOW_TECHNIQUE_POINT_CUBE = 3;
static const uint SHADOW_TECHNIQUE_CSM = 4;

struct ShadowParameter
{
    float4x4 lightViewProjection;
    float shadowBias;
    float normalBias;
    float shadowStrength;
    uint shadowMode;
    uint shadowDebugMode;
    float padding;
};

struct ExtendedShadowParameter
{
    float4x4 cascadeLightViewProjection[4];
    float4 cascadeSplits;
    float4 pointLightPositionAndFar;
    float4 cameraPositionAndPointNear;
    uint shadowTechnique; // 0:Off 1:Directional 2:SpotLinear 3:PointCube 4:CSM
    uint cascadeCount;
    uint shadowCasterLightIndex;
    uint padding0;
    float shadowBias;
    float normalBias;
    float shadowStrength;
    float padding1;
};

float ResolveShadowVisibility(float visibility, float strength)
{
    return lerp(1.0f - saturate(strength), 1.0f, visibility);
}

float CalculateReceiverSlope(float NoL)
{
    const float clampedNoL = saturate(NoL);
    const float safeNoL = max(clampedNoL, 0.20f);
    const float sinTheta = sqrt(saturate(1.0f - clampedNoL * clampedNoL));
    return min(sinTheta / safeNoL, kMaximumReceiverSlope);
}

float ResolveReceiverDepthBias(float configuredBias, float NoL, float distanceScale)
{
    const float baseBias = max(configuredBias, kMinimumReceiverDepthBias);
    const float slopeBoost = 1.0f + CalculateReceiverSlope(NoL) * kReceiverSlopeBiasScale;
    return baseBias * max(distanceScale, 1.0f) * slopeBoost; // 光へ斜めな面だけBiasを増やし、正面の影が浮く量を抑える。
}

float ResolveReceiverNormalBias(float configuredNormalBias, float NoL, float distanceScale)
{
    const float grazingWeight = saturate(1.0f - NoL);
    return max(configuredNormalBias, 0.0f) * max(distanceScale, 1.0f) * grazingWeight;
}

float SampleShadowMapPCF(
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler,
    float2 uv,
    float compareDepth)
{
    uint shadowWidth, shadowHeight;
    shadowMap.GetDimensions(shadowWidth, shadowHeight);
    const float2 texelSize = 1.0f / max(float2(shadowWidth, shadowHeight), float2(1.0f, 1.0f));

    float visibility = 0.0f;
    [unroll]
    for (int y = -kPCFRadius; y <= kPCFRadius; ++y)
    {
        [unroll]
        for (int x = -kPCFRadius; x <= kPCFRadius; ++x)
        {
            visibility += shadowMap.SampleCmpLevelZero(
                shadowSampler,
                uv + float2(x, y) * texelSize,
                compareDepth);
        }
    }
    return visibility / 9.0f;
}

float CalculateProjectedShadowPCF(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    float4x4 lightViewProjection,
    float shadowBias,
    float normalBias,
    float shadowStrength,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    const float3 surfaceNormal = normalize(normal);
    const float3 surfaceLightDir = normalize(lightDir);
    const float NoL = saturate(dot(surfaceNormal, surfaceLightDir));
    const float resolvedNormalBias = ResolveReceiverNormalBias(normalBias, NoL, 1.0f);
    const float3 biasedWorldPosition = worldPosition + surfaceNormal * resolvedNormalBias;
    const float4 shadowPosition = mul(float4(biasedWorldPosition, 1.0f), lightViewProjection);

    if (shadowPosition.w <= kEpsilon)
    {
        return 1.0f;
    }

    const float3 projected = shadowPosition.xyz / shadowPosition.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f) || projected.z < 0.0f || projected.z > 1.0f)
    {
        return 1.0f;
    }

    const float resolvedDepthBias = ResolveReceiverDepthBias(shadowBias, NoL, 1.0f);
    const float visibility = SampleShadowMapPCF(shadowMap, shadowSampler, uv, projected.z - resolvedDepthBias);
    return ResolveShadowVisibility(visibility, shadowStrength);
}

float CalculateShadowPCF(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    return CalculateProjectedShadowPCF(
        worldPosition,
        normal,
        lightDir,
        shadowParam.lightViewProjection,
        shadowParam.shadowBias,
        shadowParam.normalBias,
        shadowParam.shadowStrength,
        shadowMap,
        shadowSampler);
}

float CalculateShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    return CalculateShadowPCF(worldPosition, normal, lightDir, shadowParam, shadowMap, shadowSampler);
}

float CalculateDirectionalShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ExtendedShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    if (shadowParam.shadowTechnique != SHADOW_TECHNIQUE_DIRECTIONAL)
    {
        return 1.0f;
    }

    return CalculateProjectedShadowPCF(
        worldPosition,
        normal,
        lightDir,
        shadowParam.cascadeLightViewProjection[0],
        shadowParam.shadowBias,
        shadowParam.normalBias,
        shadowParam.shadowStrength,
        shadowMap,
        shadowSampler);
}

float CalculateSpotLinearShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ExtendedShadowParameter shadowParam,
    Texture2D<float> shadowMap,
    SamplerComparisonState shadowSampler)
{
    if (shadowParam.shadowTechnique != SHADOW_TECHNIQUE_SPOT_LINEAR)
    {
        return 1.0f;
    }

    const float3 lightPosition = shadowParam.pointLightPositionAndFar.xyz;
    const float farZ = max(shadowParam.pointLightPositionAndFar.w, 0.02f);
    const float nearZ = clamp(shadowParam.cameraPositionAndPointNear.w, 0.01f, farZ * 0.5f);
    const float3 surfaceNormal = normalize(normal);
    const float3 surfaceLightDir = normalize(lightDir);
    const float NoL = saturate(dot(surfaceNormal, surfaceLightDir));
    const float resolvedNormalBias = ResolveReceiverNormalBias(shadowParam.normalBias, NoL, 1.0f);
    const float3 biasedPosition = worldPosition + surfaceNormal * resolvedNormalBias;
    const float4 shadowPosition = mul(float4(biasedPosition, 1.0f), shadowParam.cascadeLightViewProjection[0]);

    if (shadowPosition.w <= kEpsilon)
    {
        return 1.0f;
    }

    const float3 projected = shadowPosition.xyz / shadowPosition.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
    const float distanceToLight = length(biasedPosition - lightPosition);
    if (any(uv < 0.0f) || any(uv > 1.0f) || distanceToLight <= nearZ || distanceToLight >= farZ)
    {
        return 1.0f;
    }

    const float minimumWorldBias = 0.01f / farZ;
    const float resolvedDepthBias = ResolveReceiverDepthBias(max(shadowParam.shadowBias, minimumWorldBias), NoL, 1.0f);
    const float compareDepth = saturate(distanceToLight / farZ) - resolvedDepthBias;
    const float visibility = SampleShadowMapPCF(shadowMap, shadowSampler, uv, compareDepth);
    return ResolveShadowVisibility(visibility, shadowParam.shadowStrength);
}

uint SelectShadowCascade(float cameraDepth, ExtendedShadowParameter shadowParam)
{
    uint cascadeIndex = 0;
    cascadeIndex += cameraDepth > shadowParam.cascadeSplits.x;
    cascadeIndex += cameraDepth > shadowParam.cascadeSplits.y;
    cascadeIndex += cameraDepth > shadowParam.cascadeSplits.z;
    return min(cascadeIndex, max(shadowParam.cascadeCount, 1u) - 1u);
}

float CalculateCsmCameraDepth(float3 worldPosition, ExtendedShadowParameter shadowParam)
{
    const float3 cameraForward = shadowParam.pointLightPositionAndFar.xyz;
    const float forwardLengthSq = dot(cameraForward, cameraForward);
    if (forwardLengthSq <= kEpsilon)
    {
        return length(worldPosition - shadowParam.cameraPositionAndPointNear.xyz);
    }

    return max(dot(worldPosition - shadowParam.cameraPositionAndPointNear.xyz, normalize(cameraForward)), 0.0f);
}

float CalculateCsmShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ExtendedShadowParameter shadowParam,
    Texture2DArray<float> csmShadowMaps,
    SamplerComparisonState shadowSampler)
{
    if (shadowParam.shadowTechnique != SHADOW_TECHNIQUE_CSM || shadowParam.cascadeCount == 0)
    {
        return 1.0f;
    }

    const float cameraDepth = CalculateCsmCameraDepth(worldPosition, shadowParam);
    const uint cascadeIndex = SelectShadowCascade(cameraDepth, shadowParam);
    const float firstCascadeEnd = max(shadowParam.cascadeSplits.x, kEpsilon);
    const float currentCascadeEnd = max(shadowParam.cascadeSplits[cascadeIndex], firstCascadeEnd);
    const float cascadeBiasScale = clamp(sqrt(currentCascadeEnd / firstCascadeEnd), 1.0f, 3.0f);
    const float3 surfaceNormal = normalize(normal);
    const float3 surfaceLightDir = normalize(lightDir);
    const float NoL = saturate(dot(surfaceNormal, surfaceLightDir));
    const float resolvedNormalBias = ResolveReceiverNormalBias(shadowParam.normalBias, NoL, cascadeBiasScale);
    const float3 biasedPosition = worldPosition + surfaceNormal * resolvedNormalBias;
    const float4 shadowPosition = mul(float4(biasedPosition, 1.0f), shadowParam.cascadeLightViewProjection[cascadeIndex]);
    if (shadowPosition.w <= kEpsilon) { return 1.0f; }

    const float3 projected = shadowPosition.xyz / shadowPosition.w;
    const float2 uv = float2(projected.x * 0.5f + 0.5f, -projected.y * 0.5f + 0.5f);
    if (any(uv < 0.0f) || any(uv > 1.0f) || projected.z < 0.0f || projected.z > 1.0f) { return 1.0f; }

    uint width, height, layers;
    csmShadowMaps.GetDimensions(width, height, layers);
    const float2 texelSize = 1.0f / max(float2(width, height), float2(1.0f, 1.0f));
    const float resolvedDepthBias = ResolveReceiverDepthBias(shadowParam.shadowBias, NoL, cascadeBiasScale);
    float visibility = 0.0f;
    [unroll]
    for (int y = -kPCFRadius; y <= kPCFRadius; ++y)
    {
        [unroll]
        for (int x = -kPCFRadius; x <= kPCFRadius; ++x)
        {
            visibility += csmShadowMaps.SampleCmpLevelZero(
                shadowSampler,
                float3(uv + float2(x, y) * texelSize, (float)cascadeIndex),
                projected.z - resolvedDepthBias);
        }
    }

    return ResolveShadowVisibility(visibility / 9.0f, shadowParam.shadowStrength);
}

float CalculatePointCubeShadow(
    float3 worldPosition,
    float3 normal,
    float3 lightDir,
    ExtendedShadowParameter shadowParam,
    TextureCube<float> pointShadowMap,
    SamplerComparisonState shadowSampler)
{
    if (shadowParam.shadowTechnique != SHADOW_TECHNIQUE_POINT_CUBE) { return 1.0f; }

    const float3 lightPosition = shadowParam.pointLightPositionAndFar.xyz;
    const float farZ = max(shadowParam.pointLightPositionAndFar.w, 0.02f);
    const float nearZ = clamp(shadowParam.cameraPositionAndPointNear.w, 0.01f, farZ * 0.5f);
    const float3 surfaceNormal = normalize(normal);
    const float3 surfaceLightDir = normalize(lightDir);
    const float NoL = saturate(dot(surfaceNormal, surfaceLightDir));
    const float resolvedNormalBias = ResolveReceiverNormalBias(shadowParam.normalBias, NoL, 1.0f);
    const float3 biasedPosition = worldPosition + surfaceNormal * resolvedNormalBias;
    const float3 fromLight = biasedPosition - lightPosition;
    const float distanceToLight = length(fromLight);
    if (distanceToLight <= nearZ || distanceToLight >= farZ) { return 1.0f; }

    const float minimumWorldBias = 0.01f / farZ;
    const float resolvedDepthBias = ResolveReceiverDepthBias(max(shadowParam.shadowBias, minimumWorldBias), NoL, 1.0f);
    const float compareDepth = saturate(distanceToLight / farZ) - resolvedDepthBias;
    const float3 sampleDirection = normalize(fromLight);

    uint shadowWidth, shadowHeight, mipLevels;
    pointShadowMap.GetDimensions(0, shadowWidth, shadowHeight, mipLevels);
    const float texelAngle = 2.0f / max((float)shadowWidth, 1.0f);
    const float3 referenceUp = abs(sampleDirection.y) > 0.98f
        ? float3(1.0f, 0.0f, 0.0f)
        : float3(0.0f, 1.0f, 0.0f);
    const float3 tangent = normalize(cross(referenceUp, sampleDirection));
    const float3 bitangent = normalize(cross(sampleDirection, tangent));

    float visibility = pointShadowMap.SampleCmpLevelZero(shadowSampler, sampleDirection, compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection + tangent * texelAngle), compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection - tangent * texelAngle), compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection + bitangent * texelAngle), compareDepth);
    visibility += pointShadowMap.SampleCmpLevelZero(shadowSampler, normalize(sampleDirection - bitangent * texelAngle), compareDepth);
    visibility /= 5.0f; // Cube Face境界を跨いでも同じ距離基準の5tap PCFとして評価する。

    return ResolveShadowVisibility(visibility, shadowParam.shadowStrength);
}

#endif