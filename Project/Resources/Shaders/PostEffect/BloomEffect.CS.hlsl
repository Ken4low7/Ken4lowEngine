// BloomEffect: BrightExtract / Blur / Composite の最小入口。
// 高品質Bloomは複数RTと多段Blurが必要なため、今回はBarrier順序を増やさない単一Computeに留める。

struct BloomSetting
{
    float threshold; // BrightExtract: この輝度以上をにじませる。
    float intensity; // Composite: 元画像へ足すBloom量。初期値0.0で既存見た目を保つ。
    float blurStrength; // Blur: 近傍サンプルの広がり。
    float padding;
};

RWTexture2D<float4> gOutputTexture : register(u0);
Texture2D<float4> gInputTexture : register(t0);
ConstantBuffer<BloomSetting> gBloomSetting : register(b0);

float3 ExtractBright(float3 color)
{
    float luminance = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
    float amount = saturate((luminance - gBloomSetting.threshold) / max(luminance, 1e-4f));
    return color * amount;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    gOutputTexture.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h)
    {
        return;
    }

    int2 pixel = int2(DTid.xy);
    int radius = (gBloomSetting.blurStrength > 0.0f) ? 1 : 0;

    // BrightExtractはBloomの入力だけを作る責務。元画像はCompositeで必ず残す。
    float3 bloom = 0.0.xxx;
    float weightSum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            int2 samplePixel = clamp(pixel + int2(x, y) * radius, int2(0, 0), int2(w - 1, h - 1));
            float weight = (x == 0 && y == 0) ? 4.0f : ((x == 0 || y == 0) ? 2.0f : 1.0f);
            bloom += ExtractBright(gInputTexture.Load(int3(samplePixel, 0)).rgb) * weight;
            weightSum += weight;
        }
    }

    // Blurは抽出した明部を軽く広げる責務。今後はここを多段/低解像度RTへ分離する。
    bloom /= max(weightSum, 1e-4f);

    float4 source = gInputTexture.Load(int3(pixel, 0));
    // Compositeは元画像へBloomを足し戻す責務。初期intensity=0で既存PostEffectの見た目を守る。
    gOutputTexture[DTid.xy] = float4(saturate(source.rgb + bloom * gBloomSetting.intensity), source.a);
}
