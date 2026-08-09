struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
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
    float3 worldPosition : TEXCOORD0;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    const float4 worldPosition = mul(input.position, gTransformationMatrix.World);
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.worldPosition = worldPosition.xyz; // Point Shadow PSへCasterのワールド座標を渡す。
    return output;
}
