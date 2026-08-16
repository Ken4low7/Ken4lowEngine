cbuffer BladeTrailViewCB : register(b0)
{
    float4x4 gViewProjection;
};

struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), gViewProjection);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}
