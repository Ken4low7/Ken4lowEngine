#include "SkinningObject3d.hlsli"
#include "../Object3D/LightingCommon.hlsli"
#include "../Object3D/PbrDirectLighting.hlsli"

//ピクセルシェーダーの出力
struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// マテリアル
struct Material
{
    float4 color; // オブジェクトの色
    float shininess; // 光沢度
    float pbrEnabled; // 1.0でPBR Direct Lightingを使用。旧padding領域でCB互換を維持する。
    float metallic; // Metallic/Roughness Texture未接続時の定数fallback
    float normalScale; // NormalMap未接続時も設定値だけ保持する
    float4x4 uvTransform; // UVTransform
    float reflectionRate; // 反射率
    float roughness; // 粗さ
    float usePointSampling; // Object3D と同じ定数バッファレイアウトを保つ
    float occlusionStrength; // AO Texture未接続時の定数fallback
    float4 emissiveFactor; // Emissive Textureへ乗算する発光色
    uint textureFlags; // bit0:MR bit1:Normal bit2:AO bit3:Emissive
    float3 padding;
};

// カメラ
struct Camera
{
    float3 worldPosition; // カメラの位置
    float padding0;
};

struct LightInfo
{
    uint gLightCount;
    float padding0;
};

ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<Camera> gCamera : register(b1);
ConstantBuffer<LightInfo> gLightInfo : register(b2);
ConstantBuffer<ShadowParameter> gShadowParameter : register(b4);
ConstantBuffer<LightingSettings> gLightingSettings : register(b5);
ConstantBuffer<ExtendedShadowParameter> gExtendedShadowParameter : register(b6);

Texture2D<float4> gTexture : register(t0); // テクスチャ
TextureCube<float4> gEnvironmentTexture : register(t1); // 環境マップ
StructuredBuffer<PunctualLight> gPunctualLights : register(t2); // パンクチュアルライト
Texture2D<float> gShadowMap : register(t4); // シャドウマップ
Texture2D<float4> gMetallicRoughnessTexture : register(t6);
Texture2D<float4> gNormalTexture : register(t7);
Texture2D<float4> gOcclusionTexture : register(t8);
Texture2D<float4> gEmissiveTexture : register(t9);
Texture2DArray<float> gCsmShadowMaps : register(t10);
TextureCube<float> gPointShadowMap : register(t11);

SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

static const float kAlphaDiscardThreshold = 0.001f;

float ComputeFresnelSchlick(float cosTheta, float f0)
{
    return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

// ピクセルシェーダー (PS) のメイン関数 (メインエントリーポイント)
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // SRGB SRV は Sample 時点で線形化されるため、Object3D と同じく手動 pow は行わない。
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    float3 worldPosition = input.worldPosition;
    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(gCamera.worldPosition - worldPosition);

    float spotShadowFactor = 1.0f;
    if (gShadowParameter.shadowMode == 2)
    {
        float3 dominantSpotDir = float3(0.0f, -1.0f, 0.0f);
        [loop]
        for (uint i = 0; i < gLightInfo.gLightCount; ++i)
        {
            if (gPunctualLights[i].lightType == LIGHT_TYPE_SPOT)
            {
                dominantSpotDir = normalize(gPunctualLights[i].position - worldPosition);
                break;
            }
        }
        spotShadowFactor = CalculateShadow(worldPosition, normal, dominantSpotDir, gShadowParameter, gShadowMap, gShadowSampler);
    }
	else if (gShadowParameter.shadowMode == 4)
	{
		float3 directionalLightDir = float3(0.0f, 1.0f, 0.0f);
		[loop]
		for (uint directionalIndex = 0; directionalIndex < gLightInfo.gLightCount; ++directionalIndex)
		{
			if (gPunctualLights[directionalIndex].lightType == LIGHT_TYPE_DIRECTIONAL)
			{
				directionalLightDir = normalize(-gPunctualLights[directionalIndex].direction);
				break;
			}
		}
		spotShadowFactor = CalculateCsmShadow(worldPosition, normal, directionalLightDir, gExtendedShadowParameter, gCsmShadowMaps, gShadowSampler);
	}
	else if (gShadowParameter.shadowMode == 3 && gExtendedShadowParameter.shadowCasterLightIndex < gLightInfo.gLightCount)
	{
		PunctualLight pointCaster = gPunctualLights[gExtendedShadowParameter.shadowCasterLightIndex];
		spotShadowFactor = CalculatePointCubeShadow(worldPosition, normal, normalize(pointCaster.position - worldPosition), gExtendedShadowParameter, gPointShadowMap, gShadowSampler);
	}

    if (gShadowParameter.shadowDebugMode == 1)
    {
		if (gShadowParameter.shadowMode == 3)
		{
			float3 cubeDirection = normalize(worldPosition - gExtendedShadowParameter.pointLightPositionAndFar.xyz);
			float cubeDepth = gPointShadowMap.SampleLevel(gSampler, cubeDirection, 0.0f);
			output.color = float4(cubeDepth.xxx, 1.0f);
			return output;
		}
		uint debugCascade = SelectShadowCascade(length(worldPosition - gExtendedShadowParameter.cameraPositionAndPointNear.xyz), gExtendedShadowParameter);
		float4x4 debugLightViewProjection = (gShadowParameter.shadowMode == 4) ? gExtendedShadowParameter.cascadeLightViewProjection[debugCascade] : gShadowParameter.lightViewProjection;
        float4 shadowPosition = mul(float4(worldPosition, 1.0f), debugLightViewProjection);
        float3 proj = shadowPosition.xyz / max(shadowPosition.w, 1e-5f);
        float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);
        float depth = (gShadowParameter.shadowMode == 4)
			? gCsmShadowMaps.SampleLevel(gSampler, float3(saturate(uv), (float)debugCascade), 0.0f)
			: gShadowMap.SampleLevel(gSampler, saturate(uv), 0.0f);
        output.color = float4(depth.xxx, 1.0f);
        return output;
    }
    if (gShadowParameter.shadowDebugMode == 2)
    {
        output.color = float4(spotShadowFactor.xxx, 1.0f);
        return output;
    }

    float3 lighting = AccumulateLighting(gPunctualLights,
        gLightInfo.gLightCount,
        worldPosition,
        normal,
        viewDir,
        gShadowParameter,
        gMaterial.shininess,
        gShadowMap,
        gExtendedShadowParameter,
        gCsmShadowMaps,
        gPointShadowMap,
        gShadowSampler,
        gLightingSettings);

    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;

    float3 reflectionDir = reflect(-viewDir, normal);
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectionDir).rgb;
    float fresnel = ComputeFresnelSchlick(saturate(dot(normal, viewDir)), 0.02f);
    float envBlend = saturate(gMaterial.reflectionRate * 0.12f + fresnel * 0.03f);

    float3 shadedColor = 0.0.xxx;
    if (gMaterial.pbrEnabled > 0.5f)
    {
        // PBRはMaterial単位でONの時だけ使い、既存アニメーションモデルのLegacy表示を初期状態で残す。
        float2 metallicRoughness = ResolveMetallicRoughnessFallback(gMaterial.metallic, gMaterial.roughness);
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_METALLIC_ROUGHNESS) != 0)
        {
            float4 metallicRoughnessSample = gMetallicRoughnessTexture.Sample(gSampler, transformedUV.xy);
            metallicRoughness.x *= metallicRoughnessSample.b;
            metallicRoughness.y *= metallicRoughnessSample.g;
        }

        float3 pbrNormal = ResolvePbrNormalFallback(normal, gMaterial.normalScale);
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_NORMAL) != 0)
        {
            pbrNormal = ResolvePbrNormalMap(
                normal,
                worldPosition,
                transformedUV.xy,
                gNormalTexture.Sample(gSampler, transformedUV.xy).xyz,
                gMaterial.normalScale);
        }

        float occlusion = 1.0f;
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_OCCLUSION) != 0)
        {
            float sampledOcclusion = gOcclusionTexture.Sample(gSampler, transformedUV.xy).r;
            occlusion = lerp(1.0f, sampledOcclusion, saturate(gMaterial.occlusionStrength));
        }

        float3 emissiveSample = 1.0.xxx;
        if ((gMaterial.textureFlags & MATERIAL_TEXTURE_EMISSIVE) != 0)
        {
            emissiveSample = gEmissiveTexture.Sample(gSampler, transformedUV.xy).rgb;
        }

        PbrSurface surface;
        surface.baseColor = baseColor;
        surface.metallic = metallicRoughness.x;
        surface.roughness = metallicRoughness.y;
        surface.occlusion = occlusion;
        surface.emissive = gMaterial.emissiveFactor.rgb * emissiveSample;
        surface.normal = pbrNormal;
        surface.viewDir = viewDir;

        float3 directPbr = DirectLightingPBR(gPunctualLights, gLightInfo.gLightCount, worldPosition, surface, gShadowParameter, gShadowMap, gExtendedShadowParameter, gCsmShadowMaps, gPointShadowMap, gShadowSampler);
        float3 ibl = EvaluatePbrIBLFallback(surface, gEnvironmentTexture, gSampler, gLightingSettings);
        shadedColor = directPbr + ibl + surface.emissive;
    }
    else
    {
        // Legacy経路は既存Phong/Blinn系の見た目とLight/Shadow挙動を守るため残す。
        shadedColor = baseColor * lighting;
        shadedColor = lerp(shadedColor, environmentColor, envBlend);
    }

    shadedColor = ApplyFog(shadedColor, worldPosition, gCamera.worldPosition, gLightingSettings);
    shadedColor = ApplySimpleToneMapping(shadedColor, gLightingSettings);
    shadedColor = ApplyContrast(shadedColor, gLightingSettings);

    output.color.rgb = shadedColor;
    output.color.a = gMaterial.color.a * textureColor.a;

    if (output.color.a < kAlphaDiscardThreshold)
    {
        discard;
    }

    return output;
}
