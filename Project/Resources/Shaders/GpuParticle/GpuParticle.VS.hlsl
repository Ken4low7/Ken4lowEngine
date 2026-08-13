#include "GpuParticle.hlsli" //頂点シェーダーへの入力頂点構造
#include "GpuParticleData.hlsli" //パーティクルデータ構造体"

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct PerView
{
    float4x4 viewProjectionMatrix;
    float4x4 billboardMatrix;
    uint bollboardMode;
    float3 padding;
};

bool IsBillboardMode(uint mode, uint flag)
{
    return (mode & flag) != 0;
}

float3 SafeNormalize(float3 v, float3 fallbackDir)
{
    float len = length(v);
    return (len > 1e-6f) ? (v / len) : normalize(fallbackDir);
}

float4x4 MakeBasisRowMajor(float3 xAxis, float3 yAxis, float3 zAxis)
{
    return float4x4(
        xAxis.x, xAxis.y, xAxis.z, 0.0f,
        yAxis.x, yAxis.y, yAxis.z, 0.0f,
        zAxis.x, zAxis.y, zAxis.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
}

StructuredBuffer<Particle> gParticles : register(t0);
StructuredBuffer<uint> gVisibleParticleIndices : register(t1);
ConstantBuffer<PerView> gPerView : register(b0);

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    // ExecuteIndirectのinstanceIdはCompaction済みindex列を引き、死粒子をVSへ流さない。
    const uint particleIndex = gVisibleParticleIndices[instanceId];
    Particle particle = gParticles[particleIndex];
    float4x4 worldMatrix;

    if (IsBillboardMode(particle.billboardMode, BILLBOARD_RIBBON))
    {
        float3 camRight = SafeNormalize(gPerView.billboardMatrix[0].xyz, float3(1, 0, 0));
        float3 camUp = SafeNormalize(gPerView.billboardMatrix[1].xyz, float3(0, 1, 0));
        float3 camForward = SafeNormalize(gPerView.billboardMatrix[2].xyz, float3(0, 0, 1));
        float3 tangent = SafeNormalize(particle.velocity, camUp);
        float3 side = cross(camForward, tangent);
        float sideLen = length(side);
        if (sideLen <= 1e-5f)
        {
            side = camRight;
        }
        else
        {
            side /= sideLen;
        }
        float3 forward = SafeNormalize(cross(side, tangent), camForward);
        worldMatrix = MakeBasisRowMajor(side, tangent, forward);
    }
    else if (IsBillboardMode(particle.billboardMode, BILLBOARD_CAMERA))
    {
        worldMatrix = gPerView.billboardMatrix;
    }
    else if (IsBillboardMode(particle.billboardMode, BILLBOARD_YAXIS))
    {
        worldMatrix = gPerView.billboardMatrix;
    }
    else
    {
        worldMatrix = float4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
    }

    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz += particle.translate;

    float sinRotation;
    float cosRotation;
    sincos(particle.rotation, sinRotation, cosRotation);
    float4 localPosition = input.position;
    localPosition.xy = float2(
        input.position.x * cosRotation - input.position.y * sinRotation,
        input.position.x * sinRotation + input.position.y * cosRotation);

    output.position = mul(localPosition, mul(worldMatrix, gPerView.viewProjectionMatrix));
    output.texcoord = input.texcoord;
    output.color = particle.color;
    output.type = particle.type;
    output.atlasCols = particle.atlasCols;
    output.atlasRows = particle.atlasRows;
    output.animFrameCount = particle.animFrameCount;
    output.animFps = particle.animFps;
    output.currentTime = particle.currentTime;
    output.animFlags = particle.animFlags;
    output.startFrame = particle.startFrame;
    output.animSpeed = particle.animSpeed;
    output.renderGroup = particle.type;

    return output;
}
