#include "Object3d.hlsli"
#include "ShadowCommon.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float4 worldPosition = mul(input.position, gTransformationMatrix.World);

    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = worldPosition.xyz;
    output.shadowPosition = mul(worldPosition, gShadowParameter.lightViewProjection);
	output.instanceColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    return output;
}
