struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInversedTranspose;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

float4 main(VertexShaderInput input) : SV_POSITION
{
    // Object-ID Passでは形状と深度だけが必要なため、WVP変換だけを行う。
    return mul(input.position, gTransformationMatrix.WVP);
}
