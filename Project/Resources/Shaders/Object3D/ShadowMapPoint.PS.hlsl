struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
};

struct PointShadowPass
{
    float4 lightPositionAndFar;
};

struct PixelShaderOutput
{
    float depth : SV_Depth;
};

ConstantBuffer<PointShadowPass> gPointShadowPass : register(b1);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    const float farZ = max(gPointShadowPass.lightPositionAndFar.w, 0.001f);
    const float distanceToLight = length(input.worldPosition - gPointShadowPass.lightPositionAndFar.xyz);
    output.depth = saturate(distanceToLight / farZ); // Face依存のProjection深度ではなく6面共通の線形距離を保存する。
    return output;
}
