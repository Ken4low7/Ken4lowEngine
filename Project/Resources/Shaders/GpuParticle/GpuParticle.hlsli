struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;

    nointerpolation uint type : TEXCOORD1;

    nointerpolation uint atlasCols : TEXCOORD2;
    nointerpolation uint atlasRows : TEXCOORD3;
    nointerpolation uint animFrameCount : TEXCOORD4;
    nointerpolation float animFps : TEXCOORD5;
    nointerpolation float currentTime : TEXCOORD6;
    nointerpolation uint animFlags : TEXCOORD7;
    nointerpolation uint startFrame : TEXCOORD8;
    nointerpolation float animSpeed : TEXCOORD9;
};