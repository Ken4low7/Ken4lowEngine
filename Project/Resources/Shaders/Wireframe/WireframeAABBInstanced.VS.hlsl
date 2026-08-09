#include "Wireframe.hlsli"

struct VertexShaderInput
{
    float3 position : POSITION0;
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
    float4 color : COLOR0;
};

struct TransformationMatrix
{
    float4x4 viewProjection;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

// 形状ごとの共有ローカル頂点をインスタンスのworld行列で変換し、共通のViewProjectionを適用する。
VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    const float4x4 world = float4x4(input.world0, input.world1, input.world2, input.world3);
    const float4 worldPosition = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPosition, gTransformationMatrix.viewProjection);
    output.color = input.color;
    return output;
}
