struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(PSInput input) : SV_TARGET0
{
    const float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    if (textureColor.a < 0.02f)
    {
        discard;
    }
    return textureColor * input.color;
}

