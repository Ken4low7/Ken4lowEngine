// スムージングエフェクトのコンピュートシェーダー

struct SmoothingSetting
{
    int kernelType; // カーネルサイズ（3, 5, 7, 9）
};

RWTexture2D<float4> gOutputTexture : register(u0); // 出力テクスチャ
Texture2D<float4> gInputTexture : register(t0); // 入力テクスチャ
SamplerState gSampler : register(s0); // Box/Gaussianに応じて将来使う可能性あり
ConstantBuffer<SmoothingSetting> gSmoothingSetting : register(b0); // スムージング設定

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 coord = DTid.xy;

    uint width, height;
    gInputTexture.GetDimensions(width, height);
    if (coord.x >= width || coord.y >= height)
        return;
    
    float2 texSize = float2(width, height);

    if (gSmoothingSetting.kernelType < 1 || gSmoothingSetting.kernelType > 7)
    {
        gOutputTexture[coord] = gInputTexture.Load(int3(coord, 0));
        return;
    }
    
    float4 color = float4(0, 0, 0, 0);
    int halfSize = 0;

    if (gSmoothingSetting.kernelType == 1)
    {
        halfSize = 1; // box3x3
    }
    else if (gSmoothingSetting.kernelType == 2)
    {
        halfSize = 2; // box5x5
    }
    else if (gSmoothingSetting.kernelType == 3)
    {
        halfSize = 2; // gaussian5x5
    }
    else if (gSmoothingSetting.kernelType == 4)
    {
        halfSize = 3; // box7x7
    }
    else if (gSmoothingSetting.kernelType == 5)
    {
        halfSize = 3; // gaussian7x7
    }
    else if (gSmoothingSetting.kernelType == 6)
    {
        halfSize = 4; // box9x9
    }
    else if (gSmoothingSetting.kernelType == 7)
    {
        halfSize = 4; // gaussian9x9
    }
    else
    {
        gOutputTexture[coord] = gInputTexture[coord];
        return;
    }

    int kernelSize = halfSize * 2 + 1;
    float weight = 1.0 / (kernelSize * kernelSize);

    for (int y = -halfSize; y <= halfSize; ++y)
    {
        for (int x = -halfSize; x <= halfSize; ++x)
        {
            int2 sampleCoord = int2(coord) + int2(x, y);
            sampleCoord = clamp(sampleCoord, int2(0, 0), int2(texSize) - 1);
            color += gInputTexture[sampleCoord];
        }
    }

    color *= weight;
    gOutputTexture[coord] = color;
}