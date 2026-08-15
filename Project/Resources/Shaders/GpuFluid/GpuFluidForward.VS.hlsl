struct GpuFluidRenderConstants
{
	float4x4 viewProjection;
	float4 domainOriginOpacity;
	float4 domainAxisUDensityScale;
	float4 domainAxisVTemperatureScale;
	float4 smokeColor;
	float4 coldColor;
	float4 hotColor;
	float4 obstacleColor;
	uint gridWidth;
	uint gridHeight;
	uint renderMode;
	uint padding;
};

cbuffer FluidForwardCB : register(b0)
{
	GpuFluidRenderConstants gRender;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VSOutput main(uint vertexId : SV_VertexID)
{
	const float2 corners[6] =
	{
		float2(0.0f, 0.0f),
		float2(0.0f, 1.0f),
		float2(1.0f, 1.0f),
		float2(0.0f, 0.0f),
		float2(1.0f, 1.0f),
		float2(1.0f, 0.0f)
	};

	VSOutput output;
	const float2 uv = corners[vertexId % 6u];
	const float3 worldPosition =
		gRender.domainOriginOpacity.xyz +
		gRender.domainAxisUDensityScale.xyz * uv.x +
		gRender.domainAxisVTemperatureScale.xyz * uv.y;

	// Domainの2軸だけでWorld-space面を作り、Simulation解像度と描画Meshを分離する。
	output.position = mul(float4(worldPosition, 1.0f), gRender.viewProjection);
	output.uv = uv;
	return output;
}
