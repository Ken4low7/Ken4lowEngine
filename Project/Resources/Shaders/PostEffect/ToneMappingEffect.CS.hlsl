// ToneMappingEffect: HDR/LDR境界を作る最小Computeシェーダー。
// 今回はHDR RenderTarget全面移行をしないため、既存LDR入力にも安全に通せる入口として扱う。

struct ToneMappingSetting
{
    float exposure; // HDR値をLDRへ落とす前の明るさ。初期値1.0で既存見た目を保つ。
    float gamma; // 表示用ガンマ。既存SRGB経路を大きく変えないよう調整可能にする。
    uint toneMappingType; // 0: None, 1: Reinhard
    float padding;
};

RWTexture2D<float4> gOutputTexture : register(u0);
Texture2D<float4> gInputTexture : register(t0);
ConstantBuffer<ToneMappingSetting> gToneMappingSetting : register(b0);

float3 ApplyToneMapping(float3 color)
{
    // ToneMappingはBloom/PBR後の最終LDR変換点になるため、まず露出だけを共通入口にする。
    float3 exposed = max(color * gToneMappingSetting.exposure, 0.0.xxx);
    if (gToneMappingSetting.toneMappingType == 1)
    {
        exposed = exposed / (1.0.xxx + exposed);
    }

    // 既存RTはSRGB/UNORM系なので、過度な変換で初期絵が崩れないよう最終的に0-1へ収める。
    float gamma = max(gToneMappingSetting.gamma, 0.01f);
    return pow(saturate(exposed), 1.0f / gamma);
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

    float4 inputColor = gInputTexture.Load(int3(DTid.xy, 0));
    gOutputTexture[DTid.xy] = float4(ApplyToneMapping(inputColor.rgb), inputColor.a);
}
