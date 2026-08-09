struct PixelShaderInput
{
    float4 position : SV_POSITION;
    nointerpolation uint objectId : TEXCOORD1;
};

uint main(PixelShaderInput input) : SV_TARGET0
{
    return input.objectId; // VSで計算したInstance固有IDをR32_UINTへそのまま出力する。
}
