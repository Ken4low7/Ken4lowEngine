// ランダムエフェクトのシェーダーコード

struct RandomSetting
{
    float time;
    int useMultiply;
    float2 textureSize;
};

SamplerState gSampler : register(s0); // サンプラーステート
Texture2D<float4> gInputTexture : register(t0); // 入力テクスチャ
RWTexture2D<float4> gOutputTexture : register(u0); // 出力テクスチャ
ConstantBuffer<RandomSetting> gRandomSetting : register(b0); // ランダムエフェクトの設定

// 0～1の乱数を返すヘルパー関数（texcoordベース）
float rand2dTo1d(float2 uv)
{
    return frac(sin(dot(uv.xy, float2(12.9898, 78.233))) * 43758.5453);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 texSize;
    gInputTexture.GetDimensions(texSize.x, texSize.y);

    if (DTid.x >= texSize.x || DTid.y >= texSize.y)
        return;

    float2 uv = (float2(DTid.xy) + 0.5f) / float2(texSize);

    float random = rand2dTo1d(uv * gRandomSetting.time);

    float4 color = gInputTexture.Sample(gSampler, uv);
    if (gRandomSetting.useMultiply != 0)
    {
        color.rgb *= random;
    }
    else
    {
        color = float4(random, random, random, 1.0f);
    }

    gOutputTexture[DTid.xy] = color;
}