#include "SkinningObject3d.hlsli"

//頂点シェーダーへの入力頂点構造
struct VertexShaderInput
{
    //POSITIONのことをセマンティクスという
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct TransformationAnimationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

ConstantBuffer<TransformationAnimationMatrix> gTransformationAnimationMatrix : register(b0);

//頂点シェーダー
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // Skinning1結果を使って変換
    output.position = mul(input.position, gTransformationAnimationMatrix.WVP); // 正しい変換順序
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationAnimationMatrix.WorldInverseTranspose));
    output.worldPosition = mul(input.position, gTransformationAnimationMatrix.World).xyz;
    return output;
}
