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

Texture2D<float> gDensity : register(t0);
Texture2D<float> gTemperature : register(t1);
Texture2D<uint> gObstacle : register(t2);
SamplerState gLinearClampSampler : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
};

float4 DrawDensity(float2 uv)
{
	const float density = max(gDensity.SampleLevel(gLinearClampSampler, uv, 0.0f), 0.0f);
	const float alpha = saturate(density * gRender.domainAxisUDensityScale.w) *
		saturate(gRender.domainOriginOpacity.w) * gRender.smokeColor.a;
	if (alpha <= 0.0001f)
	{
		discard;
	}
	return float4(gRender.smokeColor.rgb, alpha);
}

float4 DrawTemperature(float2 uv)
{
	const float temperature = gTemperature.SampleLevel(gLinearClampSampler, uv, 0.0f);
	const float magnitude = saturate(abs(temperature) * gRender.domainAxisVTemperatureScale.w);
	const float4 tint = temperature >= 0.0f ? gRender.hotColor : gRender.coldColor;
	const float alpha = magnitude * saturate(gRender.domainOriginOpacity.w) * tint.a;
	if (alpha <= 0.0001f)
	{
		discard;
	}
	return float4(tint.rgb, alpha);
}

float4 DrawObstacle(float2 uv)
{
	const uint2 safeGridSize = uint2(max(gRender.gridWidth, 1u), max(gRender.gridHeight, 1u));
	const uint2 cell = min(
		uint2(saturate(uv) * float2(safeGridSize)),
		safeGridSize - 1u);
	if (gObstacle.Load(int3(cell, 0)) == 0u)
	{
		discard;
	}

	const float alpha = saturate(gRender.domainOriginOpacity.w) * gRender.obstacleColor.a;
	return float4(gRender.obstacleColor.rgb, alpha);
}

float4 main(PSInput input) : SV_TARGET0
{
	// 通常煙・温度診断・Solid Mask診断を同じForward PSOで切り替える。
	if (gRender.renderMode == 1u)
	{
		return DrawTemperature(input.uv);
	}
	if (gRender.renderMode == 2u)
	{
		return DrawObstacle(input.uv);
	}
	return DrawDensity(input.uv);
}
