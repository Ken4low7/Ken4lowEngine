// グレースケールエフェクトのシェーダー

struct GrayScaleSetting
{
    float4 color; // 色の変化を加えるためのRGB係数（例: 白→グレースケールそのまま）
};

RWTexture2D<float4> gOutputTexture : register(u0); // 出力テクスチャ
Texture2D<float4> gInputTexture : register(t0); // 入力テクスチャ
ConstantBuffer<GrayScaleSetting> gGrayScaleSetting : register(b0); // グレースケール設定

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    gOutputTexture.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h)
        return;
    
    // 入力テクスチャから色を取得
    float4 inputColor = gInputTexture.Load(int3(DTid.xy, 0));
    float gray = dot(inputColor.rgb, float3(0.2125f, 0.7154f, 0.0721f)); // グレースケール値を計算
    gOutputTexture[DTid.xy] = float4(gray * gGrayScaleSetting.color.rgb, inputColor.a); // 出力テクスチャにグレースケール色を設定
}