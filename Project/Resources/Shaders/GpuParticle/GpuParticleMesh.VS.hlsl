#include "GpuParticle.hlsli" //頂点シェーダーへの入力頂点構造
#include "GpuParticleData.hlsli" //パーティクルデータ構造体"

// 頂点シェーダーへの入力頂点構造
struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct PerView
{
    float4x4 viewProjectionMatrix; // ビュー射影行列
    uint billboardMode; // ビルボードモード
    float3 padding; // パディング
};

StructuredBuffer<Particle> gParticles : register(t0); // 読み取り可能なパーティクルバッファ
ConstantBuffer<PerView> gPerView : register(b0); // ビュー情報

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;
    
    Particle particle = gParticles[instanceId];

    // world変換
    // scale は拡大縮小、translate は加算移動として扱う。
    // 以前の *= particle.translate は座標を掛け算してしまい、
    // MeshParticle が正しい位置に出ない原因になる。
    float4 localPosition = input.position;
    localPosition.xyz *= particle.scale;
    localPosition.xyz += particle.translate;
    
    output.position = mul(localPosition, gPerView.viewProjectionMatrix);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    output.type = particle.type;

    // deadをPSで捨てるため alphaを0にする
    if (particle.type <= 0.0f)
    {
        output.color.a = 0.0f;
    }
    
    return output;
}