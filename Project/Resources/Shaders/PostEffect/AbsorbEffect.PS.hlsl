#include "FullScreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct AbsorbSetting
{
    float time;
    float strength;
    float2 padding; // アライメント調整
};

ConstantBuffer<AbsorbSetting> gAbsorbSetting : register(b0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float2 center = float2(0.5, 0.5);
    float2 offset = uv - center;

    // 極座標に変換
    float radius = length(offset);
    float angle = atan2(offset.y, offset.x);

    // 回転を追加（時間と強さで）
    angle += gAbsorbSetting.time * 2.0f;

    // 吸引：中心に近づける（radiusを短く）
    radius = max(0.0f, radius - gAbsorbSetting.strength * 0.5f * gAbsorbSetting.time);

    // 極座標から再びUV空間に変換
    float2 spiralUV = float2(cos(angle), sin(angle)) * radius + center;

    // 色取得
    float4 texColor = gTexture.Sample(gSampler, spiralUV);

    // 色変化（中心に近いほど白くなる）
    float fade = saturate(1.0f - radius * 2.0f);
    texColor.rgb = lerp(texColor.rgb, float3(1.0, 1.0, 1.0), fade * 0.5f);

    output.color = texColor;
    return output;
}
