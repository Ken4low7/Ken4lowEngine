// ラジアルブラーのコンピュートシェーダー

struct RadialBlurSetting
{
    float2 center; // ブラーの中心位置
    float blurStrength; // ブラーの強さ
    float sampleCount; // サンプル数
};

RWTexture2D<float4> gOutputTexture : register(u0); // 出力先UAV
Texture2D<float4> gInputTexture : register(t0); // 入力元SRV
SamplerState samplerLinear : register(s0); // サンプラ
ConstantBuffer<RadialBlurSetting> gRadialBlur : register(b0); // ラジアルブラー設定

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 texSize;
    gOutputTexture.GetDimensions(texSize.x, texSize.y);
    int2 pos = DTid.xy;

    if (pos.x >= texSize.x || pos.y >= texSize.y)
        return;

    float2 uv = (float2) pos / texSize;
    float2 dir = uv - gRadialBlur.center;

    float4 result = float4(0, 0, 0, 0);
    float count = gRadialBlur.sampleCount;
    float2 step = dir * gRadialBlur.blurStrength / count;

    for (float i = 0.0; i < count; i += 1.0)
    {
        float2 sampleUV = uv - step * i;
        result += gInputTexture.SampleLevel(samplerLinear, sampleUV, 0);
    }

    result /= count;
    gOutputTexture[pos] = result;
}