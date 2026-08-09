#include "FullScreen.hlsli"

struct DepthOutlineSetting
{
    float2 texelSize; // テクセルサイズ（1/画面解像度）
    float edgeStrength; // エッジの強さ
    float threshold; // 閾値
    float4 edgeColor; // アウトラインの色
    float4x4 projectionInverse; // ← new 追加（NDC→View）
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 3×3のインデックス
static const float2 kIndex3x3[3][3] =
{
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f, 0.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f } },
    { { -1.0f, 1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } }
};

// Prewittフィルタの横方向のカーネル
static const float kPrewittHorizontalKernel[3][3] =
{
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f }
};

// Prewittフィルタの縦方向のカーネル
static const float kPrewittVerticalKernel[3][3] =
{
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f }
};

// RGBを輝度に変換する関数
float Luminance(float3 v)
{
    return dot(v, float3(0.2125f, 0.7154f, 0.0721f));
}

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

Texture2D<float> gDepthTexture : register(t1); // 深度テクスチャ
SamplerState gDepthSampler : register(s1); // 深度サンプラー

ConstantBuffer<DepthOutlineSetting> gDepthOutlineSetting : register(b0);

// ピクセルシェーダー
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float2 uv = input.texcoord;

    // Prewitt 用の深度値行列
    float gx = 0.0f, gy = 0.0f;

    // ★ 追記：距離に応じて太さをスケーリング（近いほど太く／遠いほど細く）
    float centerNdcDepth = gDepthTexture.Sample(gDepthSampler, uv);
    float4 centerNdcPos = float4(0.0f, 0.0f, centerNdcDepth, 1.0f);
    float4 centerView = mul(centerNdcPos, gDepthOutlineSetting.projectionInverse);
    float centerViewZ = abs(centerView.z / centerView.w); // ビュー空間の距離（正値）

    static const float kRef = 10.0f; // 10m で“基準太さ”
    float thicknessScale = clamp(kRef / max(centerViewZ, 1e-3f), 0.5f, 2.0f);
    
    [unroll]
    for (int y = 0; y < 3; ++y)
    {
        [unroll]
        for (int x = 0; x < 3; ++x)
        {
            //float2 off = kIndex3x3[y][x] * gDepthOutlineSetting.texelSize;
            // 太さを距離で可変に
            float2 off = kIndex3x3[y][x] * (gDepthOutlineSetting.texelSize * thicknessScale);
            
            float ndcDepth = gDepthTexture.Sample(gDepthSampler, uv + off);

            // NDC(0-1) → View‐Space Z（線形）へ変換
            float4 ndcPos = float4(0.0f, 0.0f, ndcDepth, 1.0f);
            float4 view = mul(ndcPos, gDepthOutlineSetting.projectionInverse);
            float viewZ = view.z / view.w; // 近 : −値, 遠 : −大値

            gx += viewZ * kPrewittHorizontalKernel[y][x];
            gy += viewZ * kPrewittVerticalKernel[y][x];
        }
    }

    float weight = sqrt(gx * gx + gy * gy) * gDepthOutlineSetting.edgeStrength;

    // しきい値とサチュレーション
    weight = (weight > gDepthOutlineSetting.threshold) ? weight : 0.0f;
    weight = saturate(weight);

    // 線色で合成
    float3 src = gTexture.Sample(gSampler, uv).rgb;
    float3 dst = lerp(src, gDepthOutlineSetting.edgeColor.rgb, weight);

    output.color = float4(dst, 1.0f);
    return output;
}