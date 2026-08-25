#include "Object3d.hlsli"
#include "ShadowCommon.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
    float4 waterGerstner0;
    float4 waterGerstner1;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

void AccumulateGerstnerWave(
    float2 basePosition,
    float2 direction,
    float amplitude,
    float wavelength,
    float speed,
    float steepness,
    float time,
    float phaseOffset,
    inout float2 horizontalOffset,
    inout float height,
    inout float2 heightGradient)
{
    const float twoPi = 6.28318530718f;
    float safeWavelength = max(wavelength, 0.001f);
    float waveNumber = twoPi / safeWavelength;
    float2 waveDirection = normalize(direction);
    float phase = waveNumber * dot(waveDirection, basePosition) - time * speed + phaseOffset;
    float sinePhase = sin(phase);
    float cosinePhase = cos(phase);

    horizontalOffset += waveDirection * (saturate(steepness) * amplitude * cosinePhase);
    height += amplitude * sinePhase;
    heightGradient += waveDirection * (amplitude * waveNumber * cosinePhase); // 頂点変形と同じ位相から幾何法線を再構築する。
}

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float4 localPosition = input.position;
    float3 localNormal = input.normal;

    if (gTransformationMatrix.waterGerstner0.x > 0.5f)
    {
        float time = gTransformationMatrix.waterGerstner0.y;
        float amplitude = max(gTransformationMatrix.waterGerstner0.z, 0.0f);
        float wavelength = max(gTransformationMatrix.waterGerstner0.w, 0.001f);
        float2 primaryDirection = normalize(gTransformationMatrix.waterGerstner1.xy);
        float speed = max(gTransformationMatrix.waterGerstner1.z, 0.0f);
        float steepness = saturate(gTransformationMatrix.waterGerstner1.w);
        float2 horizontalOffset = float2(0.0f, 0.0f);
        float height = 0.0f;
        float2 heightGradient = float2(0.0f, 0.0f);

        AccumulateGerstnerWave(
            input.position.xy,
            primaryDirection,
            amplitude,
            wavelength,
            speed,
            steepness,
            time,
            0.0f,
            horizontalOffset,
            height,
            heightGradient);

        float2 secondaryDirection = normalize(float2(-primaryDirection.y, primaryDirection.x) + primaryDirection * 0.35f);
        AccumulateGerstnerWave(
            input.position.xy,
            secondaryDirection,
            amplitude * 0.45f,
            wavelength * 0.58f,
            speed * 1.35f,
            steepness * 0.7f,
            time,
            1.7f,
            horizontalOffset,
            height,
            heightGradient);

        localPosition.xy += horizontalOffset;
        localPosition.z += height;
        localNormal = normalize(float3(-heightGradient.x, -heightGradient.y, 1.0f));
    }

    float4 worldPosition = mul(localPosition, gTransformationMatrix.World);

    output.position = mul(localPosition, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(localNormal, (float3x3) gTransformationMatrix.WorldInverseTranspose));
    output.worldPosition = worldPosition.xyz;
    output.shadowPosition = mul(worldPosition, gShadowParameter.lightViewProjection);
	output.instanceColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    return output;
}