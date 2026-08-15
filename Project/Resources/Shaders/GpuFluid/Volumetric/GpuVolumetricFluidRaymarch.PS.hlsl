struct GpuVolumetricFluidRenderConstants
{
    float4x4 viewProjection;
    float4x4 inverseViewProjection;
    float4 cameraPositionOpacity;
    float4 domainOriginAbsorption;
    float4 domainAxisUWidth;
    float4 domainAxisVHeight;
    float4 domainAxisWDepth;
    float4 simulationScales;
    float4 emissionEarlyExitStepsMode;
    float4 gridDimensionsShadowDistance;
    float4 depthViewportAnisotropy;
    float4 lightDirectionIntensity;
    float4 lightColorScattering;
    float4 ambientSelfShadow;
    float4 smokeColor;
    float4 coldColor;
    float4 hotColor;
    float4 obstacleColor;
};

cbuffer VolumetricFluidRenderCB : register(b0)
{
    GpuVolumetricFluidRenderConstants gRender;
};

Texture3D<float> gDensity : register(t0);
Texture3D<float> gTemperature : register(t1);
Texture3D<uint> gObstacle : register(t2);
Texture2D<float> gSceneDepth : register(t3);
SamplerState gLinearClampSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

static const uint kRenderModeSmoke = 0u;
static const uint kRenderModeDensityDebug = 1u;
static const uint kRenderModeTemperatureDebug = 2u;
static const uint kRenderModeObstacleDebug = 3u;

float3 GetVolumeExtent()
{
    return float3(
        gRender.domainAxisUWidth.w,
        gRender.domainAxisVHeight.w,
        gRender.domainAxisWDepth.w);
}

float3 WorldToVolumeDistance(float3 worldPosition)
{
    const float3 offset = worldPosition - gRender.domainOriginAbsorption.xyz;
    return float3(
        dot(offset, gRender.domainAxisUWidth.xyz),
        dot(offset, gRender.domainAxisVHeight.xyz),
        dot(offset, gRender.domainAxisWDepth.xyz));
}

bool IsInsideVolumeDistance(float3 localDistance)
{
    const float3 extent = GetVolumeExtent();
    return all(localDistance >= 0.0f) && all(localDistance <= extent);
}

bool IntersectSlab(
    float origin,
    float direction,
    float extent,
    inout float tNear,
    inout float tFar)
{
    if (abs(direction) < 1.0e-6f)
    {
        return origin >= 0.0f && origin <= extent;
    }

    float t0 = (0.0f - origin) / direction;
    float t1 = (extent - origin) / direction;
    if (t0 > t1)
    {
        const float temp = t0;
        t0 = t1;
        t1 = temp;
    }

    tNear = max(tNear, t0);
    tFar = min(tFar, t1);
    return tNear <= tFar;
}

bool IntersectVolume(
    float3 rayOriginWorld,
    float3 rayDirectionWorld,
    out float tNear,
    out float tFar,
    out bool cameraInside)
{
    const float3 origin = WorldToVolumeDistance(rayOriginWorld);
    const float3 direction = float3(
        dot(rayDirectionWorld, gRender.domainAxisUWidth.xyz),
        dot(rayDirectionWorld, gRender.domainAxisVHeight.xyz),
        dot(rayDirectionWorld, gRender.domainAxisWDepth.xyz));
    const float3 extent = GetVolumeExtent();

    cameraInside = IsInsideVolumeDistance(origin);
    tNear = -1.0e30f;
    tFar = 1.0e30f;

    return IntersectSlab(origin.x, direction.x, extent.x, tNear, tFar) &&
        IntersectSlab(origin.y, direction.y, extent.y, tNear, tFar) &&
        IntersectSlab(origin.z, direction.z, extent.z, tNear, tFar) &&
        tFar >= 0.0f;
}

float3 WorldToUvw(float3 worldPosition)
{
    const float3 localDistance = WorldToVolumeDistance(worldPosition);
    return saturate(localDistance / GetVolumeExtent());
}

uint LoadObstacle(float3 uvw)
{
    const uint3 dimensions = max(
        uint3(gRender.gridDimensionsShadowDistance.xyz),
        uint3(1u, 1u, 1u));
    const uint3 cell = min(
        uint3(saturate(uvw) * float3(dimensions)),
        dimensions - 1u);
    return gObstacle.Load(int4(cell, 0));
}

float ReconstructSceneRayDistance(
    float2 pixelPosition,
    float3 rayDirection,
    out bool hasOpaqueSurface)
{
    const float2 viewportSize = max(gRender.depthViewportAnisotropy.xy, float2(1.0f, 1.0f));
    const uint2 maxPixel = uint2(viewportSize) - 1u;
    const uint2 pixel = min(uint2(max(pixelPosition, 0.0f)), maxPixel);
    const float sceneDepth = gSceneDepth.Load(int3(pixel, 0));
    const float clearDepth = gRender.depthViewportAnisotropy.z;

    // Clear値は「このPixelにOpaque Surfaceなし」を表し、standard/reversed depthのどちらでも同じ判定を使う。
    if (abs(sceneDepth - clearDepth) <= 1.0e-5f)
    {
        hasOpaqueSurface = false;
        return 1.0e30f;
    }

    // SV_POSITION.xyは既にPixel centerなので、そのままViewport正規化してNDCへ戻す。
    const float2 uv = pixelPosition / viewportSize;
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    const float4 clipPosition = float4(ndc, sceneDepth, 1.0f);
    const float4 worldH = mul(clipPosition, gRender.inverseViewProjection);
    if (abs(worldH.w) <= 1.0e-6f)
    {
        hasOpaqueSurface = false;
        return 1.0e30f;
    }

    const float3 sceneWorld = worldH.xyz / worldH.w;
    const float rayDistance = dot(sceneWorld - gRender.cameraPositionOpacity.xyz, rayDirection);
    hasOpaqueSurface = rayDistance > 0.0f;
    return hasOpaqueSurface ? rayDistance : 1.0e30f;
}

float EvaluateHenyeyGreenstein(float cosTheta, float anisotropy)
{
    const float g = clamp(anisotropy, -0.949f, 0.949f);
    const float gSquared = g * g;
    const float denominator = max(1.0f + gSquared - 2.0f * g * cosTheta, 1.0e-4f);
    return (1.0f - gSquared) / (12.5663706f * pow(denominator, 1.5f));
}

float EvaluateLightTransmittance(
    float3 sampleWorld,
    float3 lightDirectionToLight,
    float density,
    float cellSize)
{
    if (gRender.lightDirectionIntensity.w <= 0.0f || gRender.ambientSelfShadow.w <= 0.0f)
    {
        return 1.0f;
    }

    const float shadowDistance =
        cellSize * max(gRender.gridDimensionsShadowDistance.w, 0.01f);
    const float3 shadowWorld = sampleWorld + lightDirectionToLight * shadowDistance;
    const float3 shadowLocal = WorldToVolumeDistance(shadowWorld);
    if (!IsInsideVolumeDistance(shadowLocal))
    {
        return 1.0f;
    }

    const float shadowDensity = max(
        0.0f,
        gDensity.SampleLevel(gLinearClampSampler, WorldToUvw(shadowWorld), 0.0f) *
            gRender.simulationScales.z);
    const float meanDensity = 0.5f * (density + shadowDensity);
    const float transmittance = exp(
        -meanDensity * max(gRender.domainOriginAbsorption.w, 0.0f) * shadowDistance);
    return lerp(1.0f, transmittance, saturate(gRender.ambientSelfShadow.w));
}

float4 main(PSInput input) : SV_TARGET
{
    const float3 cameraPosition = gRender.cameraPositionOpacity.xyz;
    const float3 toProxySurface = input.worldPosition - cameraPosition;
    const float proxyDistance = length(toProxySurface);
    if (proxyDistance <= 1.0e-6f)
    {
        discard;
    }

    const float3 rayDirection = toProxySurface / proxyDistance;
    float tNear = 0.0f;
    float tFar = 0.0f;
    bool cameraInside = false;
    if (!IntersectVolume(cameraPosition, rayDirection, tNear, tFar, cameraInside))
    {
        discard;
    }

    // Cull Noneでも同じRayを表裏2回積分しないよう、Volume外ではentry面、内部ではexit面だけを残す。
    const float targetProxyDistance = cameraInside ? tFar : max(tNear, 0.0f);
    const float proxyTolerance = max(gRender.simulationScales.x * 0.25f, 0.002f);
    if (abs(proxyDistance - targetProxyDistance) > proxyTolerance)
    {
        discard;
    }

    const float marchStart = max(tNear, 0.0f);
    bool hasOpaqueSurface = false;
    const float sceneRayDistance = ReconstructSceneRayDistance(
        input.position.xy,
        rayDirection,
        hasOpaqueSurface);
    const float marchEnd = hasOpaqueSurface ? min(tFar, sceneRayDistance) : tFar;
    const float rayLength = marchEnd - marchStart;
    if (rayLength <= 1.0e-5f)
    {
        discard;
    }

    const float cellSize = max(gRender.simulationScales.x, 1.0e-4f);
    const float desiredStep = max(cellSize * gRender.simulationScales.y, 1.0e-4f);
    const uint maxSteps = max(1u, (uint)round(gRender.emissionEarlyExitStepsMode.z));
    const uint desiredStepCount = max(1u, (uint)ceil(rayLength / desiredStep));
    const uint stepCount = min(maxSteps, desiredStepCount);
    const float stepLength = rayLength / float(stepCount);
    const uint renderMode = (uint)round(gRender.emissionEarlyExitStepsMode.w);

    const float3 lightDirectionToLight = normalize(gRender.lightDirectionIntensity.xyz);
    const float3 viewDirectionToCamera = -rayDirection;
    const float phase = EvaluateHenyeyGreenstein(
        dot(lightDirectionToLight, viewDirectionToCamera),
        gRender.depthViewportAnisotropy.w);

    float3 accumulatedColor = 0.0f;
    float transmittance = 1.0f;

    [loop]
    for (uint stepIndex = 0u; stepIndex < stepCount; ++stepIndex)
    {
        const float t = marchStart + (float(stepIndex) + 0.5f) * stepLength;
        const float3 sampleWorld = cameraPosition + rayDirection * t;
        const float3 uvw = WorldToUvw(sampleWorld);

        float sampleAlpha = 0.0f;
        float3 sampleColor = 0.0f;

        if (renderMode == kRenderModeObstacleDebug)
        {
            if (LoadObstacle(uvw) != 0u)
            {
                const float obstacleExtinction = max(gRender.obstacleColor.a, 0.01f) * 8.0f / cellSize;
                sampleAlpha = 1.0f - exp(-obstacleExtinction * stepLength);
                sampleColor = gRender.obstacleColor.rgb;
            }
        }
        else if (renderMode == kRenderModeTemperatureDebug)
        {
            const float temperature = gTemperature.SampleLevel(gLinearClampSampler, uvw, 0.0f);
            const float temperatureScale = max(gRender.simulationScales.w, 1.0e-4f);
            const float signedTemperature = temperature / temperatureScale;
            const float temperatureAmount = saturate(abs(signedTemperature));
            if (temperatureAmount > 1.0e-6f)
            {
                const float temperature01 = saturate(0.5f + 0.5f * signedTemperature);
                sampleAlpha = 1.0f - exp(
                    -temperatureAmount * max(gRender.domainOriginAbsorption.w, 1.0f) * stepLength);
                sampleColor = lerp(gRender.coldColor.rgb, gRender.hotColor.rgb, temperature01);
            }
        }
        else
        {
            const float density = max(
                0.0f,
                gDensity.SampleLevel(gLinearClampSampler, uvw, 0.0f) * gRender.simulationScales.z);
            if (density > 1.0e-6f)
            {
                sampleAlpha = 1.0f - exp(
                    -density * max(gRender.domainOriginAbsorption.w, 0.0f) * stepLength);

                if (renderMode == kRenderModeDensityDebug)
                {
                    const float density01 = saturate(density);
                    sampleColor = float3(density01, density01, density01); // SolverのDensity量をLightingなしのgrayscaleで直接確認する。
                }
                else
                {
                    const float temperature =
                        gTemperature.SampleLevel(gLinearClampSampler, uvw, 0.0f);
                    const float temperatureScale = max(gRender.simulationScales.w, 1.0e-4f);
                    const float signedTemperature = temperature / temperatureScale;
                    const float temperature01 = saturate(0.5f + 0.5f * signedTemperature);
                    const float temperatureAmount = saturate(abs(signedTemperature));
                    const float3 thermalColor = lerp(
                        gRender.coldColor.rgb,
                        gRender.hotColor.rgb,
                        temperature01);

                    const float lightTransmittance = EvaluateLightTransmittance(
                        sampleWorld,
                        lightDirectionToLight,
                        density,
                        cellSize);
                    const float3 directScattering =
                        gRender.lightColorScattering.rgb *
                        gRender.lightDirectionIntensity.w *
                        gRender.lightColorScattering.w *
                        phase *
                        lightTransmittance;
                    const float3 scatteringLighting = gRender.ambientSelfShadow.rgb + directScattering;
                    const float3 thermalEmission =
                        thermalColor * (gRender.emissionEarlyExitStepsMode.x * temperatureAmount);

                    sampleColor = gRender.smokeColor.rgb * scatteringLighting + thermalEmission;
                }
            }
        }

        accumulatedColor += transmittance * sampleAlpha * sampleColor;
        transmittance *= 1.0f - sampleAlpha;

        if (transmittance <= gRender.emissionEarlyExitStepsMode.y)
        {
            break;
        }
    }

    const float integratedAlpha = 1.0f - transmittance;
    const float outputAlpha = integratedAlpha * gRender.cameraPositionOpacity.w;
    if (outputAlpha <= 1.0e-4f)
    {
        discard;
    }

    // ForwardのNormal BlendはSRC_ALPHAなので、積分済みpremultiplied colorをstraight colorへ戻して渡す。
    const float3 straightColor = accumulatedColor / max(integratedAlpha, 1.0e-5f);
    return float4(straightColor, saturate(outputAlpha));
}
