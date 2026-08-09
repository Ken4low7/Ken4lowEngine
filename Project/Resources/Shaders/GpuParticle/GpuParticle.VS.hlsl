#include "GpuParticle.hlsli" //頂点シェーダーへの入力頂点構造
#include "GpuParticleData.hlsli" //パーティクルデータ構造体"

// 頂点シェーダーの出力頂点構造
struct VertexShaderInput
{
    //POSITIONのことをセマンティクスという
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// 頂点シェーダーの出力頂点構造 
struct PerView
{
    float4x4 viewProjectionMatrix; // ビュー射影行列
    float4x4 billboardMatrix; // ビルボード行列
    uint bollboardMode; // ビルボードモード
    float3 padding; // パディング
};

// ビルボードモードチェック関数
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
    0.0f, 0.0f, 0.0f, 1.0f
);
}

StructuredBuffer<Particle> gParticles : register(t0); // 読み取り可能なパーティクルバッファ
ConstantBuffer<PerView> gPerView : register(b0); // ビュー情報

// 頂点シェーダー 
VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID)
{
    VertexShaderOutput output;

    Particle particle = gParticles[instanceId];
    float4x4 worldMatrix;

    // ----------------------------
    // 擬似リボン：速度方向に伸ばす
    // （入力クワッドの +Y 方向が「長手方向」になる想定）
    // ----------------------------
    if (IsBillboardMode(particle.billboardMode, BILLBOARD_RIBBON))
    {
        // billboardMatrix からカメラ軸を取る（行ベクトル想定）
        float3 camRight = SafeNormalize(gPerView.billboardMatrix[0].xyz, float3(1, 0, 0));
        float3 camUp = SafeNormalize(gPerView.billboardMatrix[1].xyz, float3(0, 1, 0));
        float3 camForward = SafeNormalize(gPerView.billboardMatrix[2].xyz, float3(0, 0, 1));

        // 速度が 0 でも落ちないように fallback は camUp
        float3 tangent = SafeNormalize(particle.velocity, camUp); // リボンの長手方向（Y軸）

        // カメラへ向くように「幅方向」を作る
        float3 side = cross(camForward, tangent);
        float sideLen = length(side);
        if (sideLen <= 1e-5f)
        {
            // tangent と camForward がほぼ平行：退避
            side = camRight;
        }
        else
        {
            side /= sideLen;
        }

        // 法線方向（Z軸）
        float3 forward = SafeNormalize(cross(side, tangent), camForward);

        worldMatrix = MakeBasisRowMajor(side, tangent, forward);
    }
    // ----------------------------
    // 通常ビルボード
    // ----------------------------
    else if (IsBillboardMode(particle.billboardMode, BILLBOARD_CAMERA))
    {
        worldMatrix = gPerView.billboardMatrix;
    }
    else if (IsBillboardMode(particle.billboardMode, BILLBOARD_YAXIS))
    {
        // 今の実装では Camera と同じ。必要なら Y軸だけ回す行列を別途作る。
        worldMatrix = gPerView.billboardMatrix;
    }
    else
    {
        worldMatrix = float4x4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );
    }

    // スケール適用（row-major）
    worldMatrix[0] *= particle.scale.x; // 幅
    worldMatrix[1] *= particle.scale.y; // 長さ（リボンならここが伸びる）
    worldMatrix[2] *= particle.scale.z;

    // 平行移動
    worldMatrix[3].xyz += particle.translate;

    // Preview DescのstartRotation / rotationSpeedをSpriteのローカルZ回転へ反映する。
    float sinRotation;
    float cosRotation;
    sincos(particle.rotation, sinRotation, cosRotation);
    float4 localPosition = input.position;
    localPosition.xy = float2(
        input.position.x * cosRotation - input.position.y * sinRotation,
        input.position.x * sinRotation + input.position.y * cosRotation);

    // 変換
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

    return output;
}
