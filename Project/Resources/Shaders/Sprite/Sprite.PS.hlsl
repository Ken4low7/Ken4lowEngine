#include "Sprite.hlsli"

struct Material
{
    float4 color; // マテリアルカラー
    float4x4 uvTransform; // UV変換行列
    uint textureIndex; // 使用するテクスチャのインデックス
};

struct EffectParams
{
    int isReloading; // リロード中かどうか
    float reloadProgress; // リロードの進捗

    int enableCrack;
    float crackProgress;

    float2 crackHitUV;
    float crackScale;
    float crackThickness;

    uint crackSeed;
    float crackIntensity;
    float2 padding;
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// 使用するテクスチャの最大数 : SRV配列のサイズに依存
static const uint textureCount = 1024;

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<EffectParams> gEffectParams : register(b1);

Texture2D<float4> gTexture[textureCount] : register(t0);
SamplerState gSampler : register(s0);

// -------------------- hash / noise --------------------
float Hash11(float n)
{
    return frac(sin(n) * 43758.5453);
}

float Hash21(float2 p, uint seed)
{
    float n = dot(p, float2(127.1, 311.7)) + (float) seed * 0.01;
    return frac(sin(n) * 43758.5453);
}

float2 Hash22(float2 p, uint seed)
{
    float n = dot(p, float2(127.1, 311.7)) + (float) seed * 0.01;
    float2 s = sin(float2(n, n + 1.0));
    return frac(s * 43758.5453); // 0..1
}

// Voronoi の「境界っぽさ」を返す（小さいほど境界）
float VoronoiEdge(float2 x, uint seed)
{
    float2 n = floor(x);
    float2 f = frac(x);

    float md1 = 1e9;
    float md2 = 1e9;

    [unroll]
    for (int j = -1; j <= 1; ++j)
    {
        [unroll]
        for (int i = -1; i <= 1; ++i)
        {
            float2 g = float2(i, j);

            // 各セル内のランダム点（-0.5..0.5 に寄せる）
            float2 o = Hash22(n + g, seed) - 0.5;

            float2 r = g + o - f;
            float d = dot(r, r);

            if (d < md1)
            {
                md2 = md1;
                md1 = d;
            }
            else if (d < md2)
            {
                md2 = d;
            }
        }
    }

    return md2 - md1;
}

float CrackMask(float2 uv)
{
    float p = saturate(gEffectParams.crackProgress);

    // 破壊が進むほど中心から広がる（UV距離）
    float dist = length(uv - gEffectParams.crackHitUV);
    float radius = lerp(0.0, 0.85, p);
    float reveal = saturate(1.0 - smoothstep(radius, radius + 0.06, dist));

    // Voronoi 境界でひび生成
    float2 v = uv * gEffectParams.crackScale;

    float e1 = VoronoiEdge(v, gEffectParams.crackSeed);
    float m1 = 1.0 - smoothstep(0.0, gEffectParams.crackThickness, e1);

    // ディテール追加（細かい層）
    float e2 = VoronoiEdge(v * 2.0, gEffectParams.crackSeed + 17u);
    float m2 = 1.0 - smoothstep(0.0, gEffectParams.crackThickness * 0.6, e2);

    float mask = saturate(m1 + m2 * 0.5);

    // ランダムに欠けさせて「枝分かれ感」
    float drop = Hash21(floor(v), gEffectParams.crackSeed + 91u);
    mask *= smoothstep(0.15, 0.90, drop);

    // 進行度で濃さも少し上げる
    float intensity = gEffectParams.crackIntensity * lerp(0.2, 1.0, p);

    return mask * reveal * intensity;
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 通常のUV変換とテクスチャ取得
    float4 transformedUV = mul(float4(input.texcoord, 0, 1), gMaterial.uvTransform);
    float4 texColor = gTexture[gMaterial.textureIndex].Sample(gSampler, transformedUV.xy);

    // デフォルトの最終色（通常表示）
    output.color = texColor * gMaterial.color;

    // ---------- リロード時だけ扇形マスク処理 ----------
    if (gEffectParams.isReloading == 1)
    {
        float2 center = float2(0.5f, 0.5f);
        float2 uv = input.texcoord;
        float2 delta = uv - center;

        float angle = atan2(delta.y, delta.x); // -pi..pi
        if (angle < 0)
            angle += 6.2831853;

        float progressAngle = saturate(gEffectParams.reloadProgress) * 6.2831853;

        float dist2 = length(delta);
        if (angle > progressAngle || dist2 > 0.5f)
        {
            output.color.a = 0.0f;
        }
    }

    // ---------- ひび割れ（画像なし） ----------
    if (gEffectParams.enableCrack == 1 && output.color.a > 0.0f)
    {
        float crack = CrackMask(input.texcoord);

        // ヒビを「暗い線」として合成（マイクラ寄り）
        output.color.rgb = lerp(output.color.rgb, 0.0.xxx, saturate(crack));
    }

    return output;
}
