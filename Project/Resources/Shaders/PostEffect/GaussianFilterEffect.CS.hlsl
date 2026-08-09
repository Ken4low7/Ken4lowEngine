// ガウシアンフィルターのコンピュートシェーダー

struct GaussianFilterSetting
{
    int kernelType; // 3,5,7,9 のいずれか（ピクセルシェーダーで分岐）
    float intensity; // 強度
    float threshold; // 閾値
    float sigma; // ガウス関数の標準偏差
    int isHorizontal; // 水平方向か垂直方向か（1: 水平, 0: 垂直）
};

RWTexture2D<float4> gOutputTexture : register(u0); // 出力テクスチャ
Texture2D<float4> gInputTexture : register(t0); // 入力テクスチャ
SamplerState samplerLinear : register(s0); // サンプラーステート
ConstantBuffer<GaussianFilterSetting> gGaussianFilterSetting : register(b0); // ガウシアンフィルター設定

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    int2 texSize;
    gOutputTexture.GetDimensions(texSize.x, texSize.y);
    int2 pos = DTid.xy;

    // 画面外チェック
    if (pos.x >= texSize.x || pos.y >= texSize.y)
        return;

    float2 uv = pos / (float2) texSize;
    float2 texelSize = 1.0 / texSize;

    int radius = gGaussianFilterSetting.kernelType; // 1=±1(3x3), 2=±2(5x5), ...
    float2 dir = (gGaussianFilterSetting.isHorizontal == 1) ? float2(texelSize.x, 0.0) : float2(0.0, texelSize.y);

    float4 color = float4(0, 0, 0, 0);
    float totalWeight = 0.0f;

    for (int i = -radius; i <= radius; ++i)
    {
        float weight = exp(-((float) (i * i)) / (2.0 * gGaussianFilterSetting.sigma * gGaussianFilterSetting.sigma));
        float2 sampleUV = uv + dir * i;

        // サンプル
        float4 sampleColor = gInputTexture.SampleLevel(samplerLinear, sampleUV, 0);

        // α値でスキップ（threshold）
        if (gGaussianFilterSetting.threshold > 0.0f && sampleColor.a < gGaussianFilterSetting.threshold)
        {
            continue;
        }

        color += sampleColor * weight;
        totalWeight += weight;
    }

    color /= max(totalWeight, 0.0001f); // division by zero guard
    color *= gGaussianFilterSetting.intensity; // 強度調整

    gOutputTexture[pos] = color;
}