// 輝度ベースのアウトラインエフェクトのコンピュートシェーダー

struct LuminanceOutlineSetting
{
    float4 color; // エッジの色
    float2 texelSize; // テクセルサイズ（1/画面解像度）
    float edgeStrength; // エッジの強さ
    float threshold; // 閾値
};

Texture2D<float4> gInputTexture : register(t0);
RWTexture2D<float4> gOutput : register(u0);
SamplerState gSampler : register(s0);
ConstantBuffer<LuminanceOutlineSetting> gLuminanceOutlineSetting : register(b0);

float GetLuminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    gOutput.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h)
        return;

    float2 texel = 1.0 / float2(w, h);
    float2 uv = (float2(DTid.xy) + 0.5f) * texel;

    float lum = GetLuminance(gInputTexture.SampleLevel(gSampler, uv, 0).rgb);

    float diff = 0.0f;
    diff += abs(lum - GetLuminance(gInputTexture.SampleLevel(gSampler, uv + float2(texel.x, 0), 0).rgb));
    diff += abs(lum - GetLuminance(gInputTexture.SampleLevel(gSampler, uv + float2(-texel.x, 0), 0).rgb));
    diff += abs(lum - GetLuminance(gInputTexture.SampleLevel(gSampler, uv + float2(0, texel.y), 0).rgb));
    diff += abs(lum - GetLuminance(gInputTexture.SampleLevel(gSampler, uv + float2(0, -texel.y), 0).rgb));

    float4 finalColor = gInputTexture.SampleLevel(gSampler, uv, 0);
    if (diff > gLuminanceOutlineSetting.threshold)
        finalColor.rgb = gLuminanceOutlineSetting.color.rgb;

    gOutput[DTid.xy] = finalColor;
}