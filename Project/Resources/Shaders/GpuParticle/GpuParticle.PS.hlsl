#include "GpuParticle.hlsli"
#include "GpuParticleData.hlsli"

struct Material
{
    float4 color;
    float4x4 uvTransform;
    uint drawType;
    float3 _pad;
};

ConstantBuffer<Material> gMaterial : register(b1);
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

uint ResolveFlipbookFrame(VertexShaderOutput input)
{
    if (input.animFrameCount <= 1 || input.atlasCols == 0 || input.atlasRows == 0)
    {
        return 0;
    }

    float played = floor(input.currentTime * input.animFps * input.animSpeed);
    uint frame = (uint) played + input.startFrame;

    bool loop = (input.animFlags & GPU_PARTICLE_ANIM_LOOP) != 0u;
    if (loop)
    {
        frame %= input.animFrameCount;
    }
    else
    {
        frame = min(frame, input.animFrameCount - 1);
    }

    return frame;
}

float2 ResolveFlipbookUV(VertexShaderOutput input, float2 baseUV)
{
    if (input.animFrameCount <= 1 || input.atlasCols == 0 || input.atlasRows == 0)
    {
        return baseUV;
    }

    uint frame = ResolveFlipbookFrame(input);
    uint col = frame % input.atlasCols;
    uint row = frame / input.atlasCols;

    float2 tileScale = float2(
        1.0f / (float) input.atlasCols,
        1.0f / (float) input.atlasRows
    );

    // 境界にじみ軽減
    float2 inset = tileScale * 0.02f;

    float2 uv = baseUV;
    uv *= (tileScale - inset * 2.0f);
    uv += inset;
    uv.x += tileScale.x * (float) col;
    uv.y += tileScale.y * (float) row;

    return uv;
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    if (input.type != gMaterial.drawType)
    {
        discard;
    }

    float2 uv = ResolveFlipbookUV(input, input.texcoord);

    float4 transformedUV = mul(float4(uv, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float4 outColor = gMaterial.color * textureColor * input.color;

    if (outColor.a < 0.001f)
    {
        discard;
    }

    return outColor;
}