// Dissolveのコンピュートシェーダー

struct DissolveSetting
{
    float threshold; // 閾値
    float edgeThickness; // エッジの厚み
    float4 edgeColor; // エッジの色
    float3 padding; // アラインメント
};

Texture2D<float4> gInputTex : register(t0); // 入力テクスチャ
Texture2D<float4> gMaskTex : register(t1); // マスクテクスチャ
SamplerState gSampler : register(s0); // サンプラー状態
ConstantBuffer<DissolveSetting> gDissolveSetting : register(b0); // 定数バッファ
RWTexture2D<float4> gOutputTex : register(u0); // 出力テクスチャ

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 coord = DTid.xy;
    float2 texSize;
    gInputTex.GetDimensions(texSize.x, texSize.y);

    if (coord.x >= texSize.x || coord.y >= texSize.y)
        return;

    float2 uv = coord / texSize;

    float4 inputColor = gInputTex.Sample(gSampler, uv);
    float maskValue = gMaskTex.Sample(gSampler, uv).r;

    float alpha = step(maskValue, gDissolveSetting.threshold);
    float edge = smoothstep(gDissolveSetting.threshold, gDissolveSetting.threshold + gDissolveSetting.edgeThickness, maskValue);
    float4 edgeCol = gDissolveSetting.edgeColor * (1.0 - edge);

    float4 finalColor = lerp(inputColor, edgeCol, 1.0 - alpha);
    gOutputTex[coord] = finalColor;
}