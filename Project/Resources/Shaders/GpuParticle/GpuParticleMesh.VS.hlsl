#include "GpuParticle.hlsli"
#include "GpuParticleData.hlsli"

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct PerView
{
    float4x4 viewProjectionMatrix;
    uint billboardMode;
    float3 padding;
};

struct Material
{
    float4 color;
    float4x4 uvTransform;
    uint drawType;
    float3 padding;
};

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);
ConstantBuffer<Material> gMaterial : register(b1);

float3 RotateEulerXYZ(float3 position, float3 rotation)
{
    float sx, cx;
    float sy, cy;
    float sz, cz;
    sincos(rotation.x, sx, cx);
    sincos(rotation.y, sy, cy);
    sincos(rotation.z, sz, cz);

    float3 xRotated = float3(
        position.x,
        position.y * cx - position.z * sx,
        position.y * sx + position.z * cx);
    float3 yRotated = float3(
        xRotated.x * cy + xRotated.z * sy,
        xRotated.y,
        -xRotated.x * sy + xRotated.z * cy);
    return float3(
        yRotated.x * cz - yRotated.y * sz,
        yRotated.x * sz + yRotated.y * cz,
        yRotated.z);
}

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output = (VertexShaderOutput) 0;
    Particle particle = gParticles[instanceId];

    // Meshは頂点数が多いため、Material group不一致と死亡ParticleをTransform前にclipして無駄なVS/Raster処理を抑える。
    if (particle.lifeTime <= 0.0f || particle.type != gMaterial.drawType)
    {
        output.position = float4(0.0f, 0.0f, -1.0f, 1.0f);
        output.renderGroup = particle.type;
        return output;
    }

    float4 localPosition = input.position;
    localPosition.xyz *= particle.scale;
    localPosition.xyz = RotateEulerXYZ(localPosition.xyz, particle.rotation3D);
    localPosition.xyz += particle.translate;

    output.position = mul(localPosition, gPerView.viewProjectionMatrix);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    output.type = particle.type;
    output.renderGroup = particle.type; // Desc OverrideはtypeをMaterial hashとして使うのでMeshでも同じ選別契約にする。

    return output;
}