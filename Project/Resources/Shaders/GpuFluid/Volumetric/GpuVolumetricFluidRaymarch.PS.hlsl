struct GpuVolumetricFluidRenderConstants
{
    float4x4 viewProjection;
    float4 cameraPositionOpacity;
    float4 domainOriginAbsorption;
    float4 domainAxisUWidth;
    float4 domainAxisVHeight;
    float4 domainAxisWDepth;
    float4 simulationScales;
    float4 emissionEarlyExitStepsMode;
    float4 gridDimensionsPadding;
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
SamplerState gLinearClampSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

float3 WorldToVolumeDistance(float3 worldPosition)
{
    const float3 offset = worldPosition - gRender.domainOriginAbsorption.xyz;
    return float3(
        dot(offset, gRender.domainAxisUWidth.xyz),
        dot(offset, gRender.domainAxisVHeight.xyz),
        dot(offset, gRender.domainAxisWDepth.xyz));
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
    const float3 extent = float3(
        gRender.domainAxisUWidth.w,
        gRender.domainAxisVHeight.w,
        gRender.domainAxisWDepth.w);

    cameraInside = all(origin >= 0.0f) && all(origin <= extent);
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
    return saturate(localDistance / float3(
        gRender.domainAxisUWidth.w,
        gRender.domainAxisVHeight.w,
        gRender.domainAxisWDepth.w));
}

uint LoadObstacle(float3 uvw)
{
    const uint3 dimensions = max(uint3(gRender.gridDimensionsPadding.xyz), uint3(1u, 1u, 1u));
    const uint3 cell = min(
        uint3(saturate(uvw) * float3(dimensions)),
        dimensions - 1u);
    return gObstacle.Load(int4(cell, 0));
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
    const float marchEnd = tFar;
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
    const bool obstacleDebug = gRender.emissionEarlyExitStepsMode.w >= 0.5f;

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

        if (obstacleDebug)
        {
            if (LoadObstacle(uvw) != 0u)
            {
                const float obstacleExtinction = max(gRender.obstacleColor.a, 0.01f) * 8.0f / cellSize;
                sampleAlpha = 1.0f - exp(-obstacleExtinction * stepLength);
                sampleColor = gRender.obstacleColor.rgb;
            }
        }
        else
        {
            const float density = max(
                0.0f,
                gDensity.SampleLevel(gLinearClampSampler, uvw, 0.0f) * gRender.simulationScales.z);
            if (density > 1.0e-6f)
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

                sampleAlpha = 1.0f - exp(
                    -density * max(gRender.domainOriginAbsorption.w, 0.0f) * stepLength);
                sampleColor = gRender.smokeColor.rgb +
                    thermalColor * (gRender.emissionEarlyExitStepsMode.x * temperatureAmount);
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
