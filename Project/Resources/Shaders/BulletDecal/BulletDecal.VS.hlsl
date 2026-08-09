struct VSInput
{
    float3 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float4 world0 : WORLD0;
    float4 world1 : WORLD1;
    float4 world2 : WORLD2;
    float4 world3 : WORLD3;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct ViewProjectionData
{
    float4x4 viewProjection;
};

ConstantBuffer<ViewProjectionData> gViewProjection : register(b0);

VSOutput main(VSInput input)
{
    VSOutput output;
    const float4x4 world = float4x4(input.world0, input.world1, input.world2, input.world3);
    output.position = mul(mul(float4(input.position, 1.0f), world), gViewProjection.viewProjection);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}

