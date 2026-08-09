#include "SkyBox.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

struct Material
{
    float4 color;
    float4x4 uvTransform;
    float4 topColor;
    float4 bottomColor;
    float4 horizonColor;
    uint textureIndex;
    uint skyType;
    float2 uvOffset;
    float cloudHeight;
    float cloudScale;
    uint textureAvailable;
    float padding;
};

static const uint textureCount = 1024;
ConstantBuffer<Material> gMaterial : register(b0);
TextureCube<float4> gTexture[textureCount] : register(t0);
Texture2D<float4> gCloudTexture[textureCount] : register(t1024);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 direction = normalize(input.texcoord);
    if (gMaterial.skyType == 0)
    {
        output.color = gMaterial.topColor * gMaterial.color;
    }
    else if (gMaterial.skyType == 1)
    {
        // 地平線付近を明るく保ち、写真に頼らないローポリ向けの空色を作る。
        float vertical = direction.y;
        float horizonBlend = saturate(abs(vertical) * 3.0f);
        float4 verticalColor = vertical >= 0.0f ? gMaterial.topColor : gMaterial.bottomColor;
        output.color = lerp(gMaterial.horizonColor, verticalColor, horizonBlend) * gMaterial.color;
    }
    else if (gMaterial.skyType == 2 && gMaterial.textureAvailable == 0)
    {
        output.color = gMaterial.horizonColor * gMaterial.color;
    }
    else
    {
        if (gMaterial.skyType == 3)
        {
            // CloudLayer は水平平面の UV をスクロールし、透明部分から背景の空を見せる。
            float2 cloudUv = frac(input.texcoord.xy + gMaterial.uvOffset);
            output.color = gCloudTexture[gMaterial.textureIndex].Sample(gSampler, cloudUv) * gMaterial.color;
        }
        else
        {
            output.color = gTexture[gMaterial.textureIndex].Sample(gSampler, direction) * gMaterial.color;
        }
    }
    return output;
}
