#ifndef KEN4LOW_GPU_SPH_SCREEN_SPACE_COMMON_HLSLI
#define KEN4LOW_GPU_SPH_SCREEN_SPACE_COMMON_HLSLI

struct GpuSphParticle
{
    float3 position;
    float density;
    float3 velocity;
    float pressure;
    float3 predictedPosition;
    float padding;
};

struct GpuSphScreenSpaceConstants
{
    float4x4 view;
    float4x4 projection;
    float4 shallowColor;
    float4 deepColor;
    float4 screenRadiusFar;
    float4 cameraParams;
    float4 opticalParams;
    float4 blurDirection;
};

cbuffer GpuSphScreenSpaceCB : register(b0)
{
    GpuSphScreenSpaceConstants gRender;
};

float2 GpuSphScreenSize()
{
    return max(gRender.screenRadiusFar.xy, float2(1.0f, 1.0f));
}

float GpuSphParticleRadius()
{
    return max(gRender.screenRadiusFar.z, 0.001f);
}

float GpuSphFarClip()
{
    return max(gRender.screenRadiusFar.w, 1.0f);
}

bool GpuSphHasFluidDepth(float depthValue)
{
    return depthValue > 0.0f && depthValue < GpuSphFarClip() * 0.999f;
}

float3 GpuSphReconstructViewPosition(float2 uv, float viewDepth)
{
    const float fovY = max(gRender.cameraParams.y, 0.001f);
    const float aspect = max(gRender.cameraParams.z, 0.001f);
    const float tanHalfFov = tan(fovY * 0.5f);
    const float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    return float3(
        ndc.x * viewDepth * tanHalfFov * aspect,
        ndc.y * viewDepth * tanHalfFov,
        viewDepth);
}

#endif // KEN4LOW_GPU_SPH_SCREEN_SPACE_COMMON_HLSLI
