struct VertexInput
{
    float3 position : POSITION0;
    uint instanceId : SV_InstanceID;
};

struct InstanceData
{
    float4x4 world;
    float4 color;
};

struct VertexOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

cbuffer ViewProjection : register(b0)
{
    float4x4 viewProjection;
};

StructuredBuffer<InstanceData> gInstances : register(t0);

VertexOutput main(VertexInput input)
{
    VertexOutput output;
    InstanceData instanceData = gInstances[input.instanceId];
    float4 worldPosition = mul(float4(input.position, 1.0f), instanceData.world);
    output.position = mul(worldPosition, viewProjection);
    output.color = instanceData.color;
    return output;
}
