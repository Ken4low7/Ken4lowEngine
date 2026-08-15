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

struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
    const float3 corners[36] =
    {
        // -Z
        float3(0, 0, 0), float3(0, 1, 0), float3(1, 1, 0),
        float3(0, 0, 0), float3(1, 1, 0), float3(1, 0, 0),
        // +Z
        float3(0, 0, 1), float3(1, 1, 1), float3(0, 1, 1),
        float3(0, 0, 1), float3(1, 0, 1), float3(1, 1, 1),
        // -X
        float3(0, 0, 0), float3(0, 0, 1), float3(0, 1, 1),
        float3(0, 0, 0), float3(0, 1, 1), float3(0, 1, 0),
        // +X
        float3(1, 0, 0), float3(1, 1, 1), float3(1, 0, 1),
        float3(1, 0, 0), float3(1, 1, 0), float3(1, 1, 1),
        // -Y
        float3(0, 0, 0), float3(1, 0, 1), float3(0, 0, 1),
        float3(0, 0, 0), float3(1, 0, 0), float3(1, 0, 1),
        // +Y
        float3(0, 1, 0), float3(0, 1, 1), float3(1, 1, 1),
        float3(0, 1, 0), float3(1, 1, 1), float3(1, 1, 0)
    };

    const float3 local = corners[vertexId % 36u];
    const float3 worldPosition =
        gRender.domainOriginAbsorption.xyz +
        gRender.domainAxisUWidth.xyz * (gRender.domainAxisUWidth.w * local.x) +
        gRender.domainAxisVHeight.xyz * (gRender.domainAxisVHeight.w * local.y) +
        gRender.domainAxisWDepth.xyz * (gRender.domainAxisWDepth.w * local.z);

    VSOutput output;
    output.position = mul(float4(worldPosition, 1.0f), gRender.viewProjection);
    output.worldPosition = worldPosition;
    return output;
}
