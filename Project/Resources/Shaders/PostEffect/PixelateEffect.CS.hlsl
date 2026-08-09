// ピクセル化エフェクト Compute Shader

struct PixelateSettingCB
{
    float2 screenSize; // (width, height)
    float blockSize; // ピクセル単位
    float strength; // 0=なし,1=フル
};

Texture2D<float4> gInput : register(t0); // 入力テクスチャ
RWTexture2D<float4> gOutput : register(u0); // 出力テクスチャ

SamplerState gLinearSampler : register(s0);
SamplerState gPointSampler : register(s1);

ConstantBuffer<PixelateSettingCB> gSettingCB : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 coord = DTid.xy;

    uint w, h;
    gOutput.GetDimensions(w, h);
    if (coord.x >= w || coord.y >= h)
        return;

    float2 screenSize = float2(w, h);

    float2 uv = (coord + 0.5) / screenSize;

    float2 block = floor(coord / gSettingCB.blockSize) * gSettingCB.blockSize + gSettingCB.blockSize * 0.5;
    float2 blockUV = block / screenSize;

    float4 original = gInput.SampleLevel(gLinearSampler, uv, 0);
    float4 pixelated = gInput.SampleLevel(gPointSampler, blockUV, 0);

    gOutput[coord] = lerp(original, pixelated, saturate(gSettingCB.strength));
}