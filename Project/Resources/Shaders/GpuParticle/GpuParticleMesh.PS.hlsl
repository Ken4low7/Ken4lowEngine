#include "GpuParticle.hlsli"

// マテリアル構造体（スプライトPSと同じ）
struct Material
{
    float4 color;
    float4x4 uvTransform;
    uint drawType;
    float3 _pad;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

ConstantBuffer<Material> gMaterial : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    if (input.type != gMaterial.drawType)
    {
        discard;
    }

    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 tex = gTexture.Sample(gSampler, transformedUV.xy);

    output.color = gMaterial.color * tex * input.color;

    if (output.color.a == 0.0f)
    {
        discard;
    }

    return output;
}