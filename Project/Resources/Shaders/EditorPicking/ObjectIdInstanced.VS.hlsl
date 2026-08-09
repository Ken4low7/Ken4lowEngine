struct InstanceData
{
    float4x4 world;
    float4x4 worldInverseTranspose;
    float4 color;
};

struct PerView
{
    float4x4 viewProjection;
};

cbuffer ObjectIdData : register(b1)
{
    uint gBaseObjectId;
    uint gAddInstanceId;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    nointerpolation uint objectId : TEXCOORD1;
};

StructuredBuffer<InstanceData> gInstances : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    const float4 worldPosition = mul(input.position, gInstances[instanceId].world);
    output.position = mul(worldPosition, gPerView.viewProjection);
    output.objectId = gBaseObjectId + (gAddInstanceId != 0 ? instanceId : 0); // 一括PickingではSV_InstanceIDを個別IDへ変換する。
    return output;
}
