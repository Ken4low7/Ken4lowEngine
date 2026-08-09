// プレイヤーHP低下時の赤ビネット / 被弾フラッシュ用コンピュートシェーダー

struct PlayerHealthSetting
{
    float4 vignetteColor;
    float lowHealthVignetteIntensity;
    float damageFlashIntensity;
    float desaturation;
    float darkenIntensity;
    float pulseSpeed;
    float pulseIntensity;
    float elapsedTime;
    float padding;
};

RWTexture2D<float4> gOutputTexture : register(u0);
Texture2D<float4> gInputTexture : register(t0);
ConstantBuffer<PlayerHealthSetting> gSetting : register(b0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint w, h;
    gOutputTexture.GetDimensions(w, h);
    if (DTid.x >= w || DTid.y >= h)
    {
        return;
    }

    float4 color = gInputTexture.Load(int3(DTid.xy, 0));
    float2 uv = (float2(DTid.xy) + 0.5f) / float2(w, h);
    float2 centered = (uv - 0.5f) * float2((float)w / max((float)h, 1.0f), 1.0f);
    float edge = smoothstep(0.28f, 0.74f, length(centered));

    float pulse = 0.0f;
    if (gSetting.pulseSpeed > 0.0f && gSetting.pulseIntensity > 0.0f)
    {
        pulse = (sin(gSetting.elapsedTime * gSetting.pulseSpeed) * 0.5f + 0.5f) * gSetting.pulseIntensity;
    }

    float vignetteAmount = saturate(gSetting.lowHealthVignetteIntensity * (1.0f + pulse));
    float3 result = color.rgb;

    float luminance = dot(result, float3(0.299f, 0.587f, 0.114f));
    result = lerp(result, luminance.xxx, saturate(gSetting.desaturation));
    result *= (1.0f - saturate(gSetting.darkenIntensity) * edge);

    result = lerp(result, gSetting.vignetteColor.rgb, saturate(edge * vignetteAmount * gSetting.vignetteColor.a));
    result = lerp(result, gSetting.vignetteColor.rgb, saturate(gSetting.damageFlashIntensity));

    gOutputTexture[DTid.xy] = float4(saturate(result), color.a);
}
