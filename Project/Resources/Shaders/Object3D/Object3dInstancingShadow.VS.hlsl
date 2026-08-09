struct InstanceData
{
    float4x4 world;
    float4x4 worldInverseTranspose;
    float4 color;
};

struct ShadowView
{
    float4x4 lightViewProjection;
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
};

ConstantBuffer<ShadowView> gShadowView : register(b0);
StructuredBuffer<InstanceData> gInstances : register(t0);

// インスタンスごとのWorld行列を現在のShadow Pass行列へ合成する。
VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    const float4 worldPosition = mul(input.position, gInstances[instanceId].world);
    output.position = mul(worldPosition, gShadowView.lightViewProjection);
    return output;
}
