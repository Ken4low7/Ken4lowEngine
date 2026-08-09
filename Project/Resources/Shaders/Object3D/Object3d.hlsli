#ifndef OBJECT3D_HLSLI
#define OBJECT3D_HLSLI

struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
	float4 shadowPosition : TEXCOORD1;
	float4 instanceColor : COLOR0;
};

#endif
