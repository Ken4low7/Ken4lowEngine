#include "Object3d.hlsli"
#include "ShadowCommon.hlsli"

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

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

ConstantBuffer<PerView> gPerView : register(b0);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);
StructuredBuffer<InstanceData> gInstances : register(t5);

// SV_InstanceID でインスタンスごとの行列を選び、Object3Dを大量生成せずGPU側でまとめて変換する。
VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    InstanceData instance = gInstances[instanceId];

    float4 worldPosition = mul(input.position, instance.world);
    output.position = mul(worldPosition, gPerView.viewProjection);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) instance.worldInverseTranspose));
    output.worldPosition = worldPosition.xyz;
    output.shadowPosition = mul(worldPosition, gShadowParameter.lightViewProjection);
    output.instanceColor = instance.color;
    return output;
}
