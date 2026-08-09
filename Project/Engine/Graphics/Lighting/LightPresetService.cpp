#include "LightPresetService.h"

#include "DirectXCommon.h"
#include "LightManager.h"
#include "JsonDataManager.h"
#include "DataAssetPresets.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	bool LightPresetService::Save(const LightManager& lightManager, const std::string& assetId)
	{
		JsonAssetEntry entry{};
		entry.type = "LightPreset";
		entry.id = assetId;
		entry.displayName = assetId;
		entry.path = "Resources/DataAssets/LightPresets/" + assetId + ".json";

		LightPreset preset{};
		if (!lightManager.punctualLights_.empty())
		{
			const auto& light = lightManager.punctualLights_.front();
			preset.directionalDirection = light.direction;
			preset.color = light.color;
			preset.intensity = light.intensity;
		}

		// 既存LightPreset JSONのキーを変えないため、従来と同じLightPreset構造体へ値を詰める。
		preset.enableShadow = lightManager.enableShadow_;
		preset.shadowBias = lightManager.shadowBias_;
		preset.normalBias = lightManager.normalBias_;
		preset.shadowStrength = lightManager.shadowStrength_;
		preset.shadowMapSize = lightManager.shadowMapSize_;
		preset.shadowWidth = lightManager.directionalShadowWidth_;
		preset.shadowHeight = lightManager.directionalShadowHeight_;
		preset.shadowNearZ = lightManager.directionalShadowNearZ_;
		preset.shadowFarZ = lightManager.directionalShadowFarZ_;
		preset.shadowFocusMode = static_cast<uint32_t>(lightManager.shadowFocusMode_);
		preset.ambientColor = lightManager.lightingSettings_.ambientColor;
		preset.fogColor = lightManager.lightingSettings_.fogColor;
		preset.exposure = lightManager.lightingSettings_.exposure;
		preset.contrast = lightManager.lightingSettings_.contrast;
		preset.fogStart = lightManager.lightingSettings_.fogStart;
		preset.fogEnd = lightManager.lightingSettings_.fogEnd;
		preset.enableFog = lightManager.lightingSettings_.enableFog;
		preset.specularStrength = lightManager.lightingSettings_.specularStrength;
		preset.diffuseStrength = lightManager.lightingSettings_.diffuseStrength;
		preset.specularPowerScale = lightManager.lightingSettings_.specularPowerScale;
		preset.rimLightStrength = lightManager.lightingSettings_.rimLightStrength;
		preset.rimLightPower = lightManager.lightingSettings_.rimLightPower;
		preset.enableRimLight = lightManager.lightingSettings_.enableRimLight;
		preset.enableHalfLambert = lightManager.lightingSettings_.enableHalfLambert;
		preset.rimLightColor = lightManager.lightingSettings_.rimLightColor;
		preset.shadingMode = lightManager.lightingSettings_.shadingMode;
		preset.enableIBL = lightManager.lightingSettings_.enableIBL;
		preset.iblDiffuseStrength = lightManager.lightingSettings_.iblDiffuseStrength;
		preset.iblSpecularStrength = lightManager.lightingSettings_.iblSpecularStrength;
		preset.ToJson(entry.data);
		return JsonDataManager::SafeSave(entry);
	}

	bool LightPresetService::ApplyByPath(LightManager& lightManager, const std::string& filePath)
	{
		JsonAssetEntry entry{};
		if (!JsonDataManager::SafeLoad(filePath, entry))
		{
			return false;
		}

		LightPreset preset{};
		preset.FromJson(entry.data);
		if (lightManager.punctualLights_.empty())
		{
			lightManager.AddDefaultDirectionalLight();
		}

		auto& light = lightManager.punctualLights_.front();
		light.lightType = 1;
		light.enabled = 1;
		light.direction = preset.directionalDirection;
		light.color = preset.color;
		light.intensity = preset.intensity;
		lightManager.enableShadow_ = preset.enableShadow;
		lightManager.shadowBias_ = preset.shadowBias;
		lightManager.normalBias_ = preset.normalBias;
		lightManager.shadowStrength_ = preset.shadowStrength;
		lightManager.shadowMapSize_ = preset.shadowMapSize;
		lightManager.directionalShadowWidth_ = preset.shadowWidth;
		lightManager.directionalShadowHeight_ = preset.shadowHeight;
		lightManager.directionalShadowNearZ_ = preset.shadowNearZ;
		lightManager.directionalShadowFarZ_ = preset.shadowFarZ;
		lightManager.shadowFocusMode_ = static_cast<LightManager::ShadowFocusMode>(preset.shadowFocusMode);
		lightManager.lightingSettings_.ambientColor = preset.ambientColor;
		lightManager.lightingSettings_.fogColor = preset.fogColor;
		lightManager.lightingSettings_.exposure = preset.exposure;
		lightManager.lightingSettings_.contrast = preset.contrast;
		lightManager.lightingSettings_.fogStart = preset.fogStart;
		lightManager.lightingSettings_.fogEnd = preset.fogEnd;
		lightManager.lightingSettings_.enableFog = preset.enableFog;
		lightManager.lightingSettings_.specularStrength = preset.specularStrength;
		lightManager.lightingSettings_.diffuseStrength = preset.diffuseStrength;
		lightManager.lightingSettings_.specularPowerScale = preset.specularPowerScale;
		lightManager.lightingSettings_.rimLightStrength = preset.rimLightStrength;
		lightManager.lightingSettings_.rimLightPower = preset.rimLightPower;
		lightManager.lightingSettings_.enableRimLight = preset.enableRimLight;
		lightManager.lightingSettings_.enableHalfLambert = preset.enableHalfLambert;
		lightManager.lightingSettings_.rimLightColor = preset.rimLightColor;
		lightManager.lightingSettings_.shadingMode = preset.shadingMode;
		lightManager.lightingSettings_.enableIBL = preset.enableIBL;
		lightManager.lightingSettings_.iblDiffuseStrength = preset.iblDiffuseStrength;
		lightManager.lightingSettings_.iblSpecularStrength = preset.iblSpecularStrength;

		// プリセットJSON由来の不正値も描画リソースへ渡る前に安全範囲へ補正する。
		const auto clampFiniteValue = [](float value, float fallback, float minValue, float maxValue)
			{
				return std::isfinite(value) ? std::clamp(value, minValue, maxValue) : fallback;
			};
		const auto sanitizeVector4Value = [](const Vector4& value, const Vector4& fallback)
			{
				return Vector4{
					std::isfinite(value.x) ? value.x : fallback.x,
					std::isfinite(value.y) ? value.y : fallback.y,
					std::isfinite(value.z) ? value.z : fallback.z,
					std::isfinite(value.w) ? value.w : fallback.w
				};
			};
		const auto normalizeShadowMapSizeValue = [](int32_t value)
			{
				constexpr int32_t kSafeSizes[] = { 512, 1024, 2048, 4096 };
				value = std::clamp(value, kSafeSizes[0], kSafeSizes[3]);
				int32_t bestSize = kSafeSizes[0];
				int32_t bestDistance = std::abs(value - bestSize);
				for (int32_t safeSize : kSafeSizes)
				{
					const int32_t distance = std::abs(value - safeSize);
					if (distance < bestDistance)
					{
						bestSize = safeSize;
						bestDistance = distance;
					}
				}
				return static_cast<uint32_t>(bestSize);
			};

		// 旧プリセットを読み込んでも描画値が破綻しないよう、既存と同じ補正範囲を維持する。
		lightManager.lightingSettings_.ambientColor = sanitizeVector4Value(lightManager.lightingSettings_.ambientColor, LightManager::LightingSettingsGPU{}.ambientColor);
		lightManager.lightingSettings_.fogColor = sanitizeVector4Value(lightManager.lightingSettings_.fogColor, LightManager::LightingSettingsGPU{}.fogColor);
		lightManager.lightingSettings_.exposure = clampFiniteValue(lightManager.lightingSettings_.exposure, 1.0f, 0.05f, 8.0f);
		lightManager.lightingSettings_.contrast = clampFiniteValue(lightManager.lightingSettings_.contrast, 1.0f, 0.05f, 4.0f);
		lightManager.lightingSettings_.fogStart = clampFiniteValue(lightManager.lightingSettings_.fogStart, 45.0f, 0.0f, 10000.0f);
		lightManager.lightingSettings_.fogEnd = clampFiniteValue(lightManager.lightingSettings_.fogEnd, 140.0f, lightManager.lightingSettings_.fogStart + 1.0f, 20000.0f);
		lightManager.lightingSettings_.specularStrength = clampFiniteValue(lightManager.lightingSettings_.specularStrength, 0.08f, 0.0f, 4.0f);
		lightManager.lightingSettings_.diffuseStrength = clampFiniteValue(lightManager.lightingSettings_.diffuseStrength, 1.0f, 0.0f, 8.0f);
		lightManager.lightingSettings_.specularPowerScale = clampFiniteValue(lightManager.lightingSettings_.specularPowerScale, 1.0f, 0.01f, 16.0f);
		lightManager.lightingSettings_.rimLightStrength = clampFiniteValue(lightManager.lightingSettings_.rimLightStrength, 0.0f, 0.0f, 8.0f);
		lightManager.lightingSettings_.rimLightPower = clampFiniteValue(lightManager.lightingSettings_.rimLightPower, 2.0f, 0.01f, 32.0f);
		lightManager.lightingSettings_.rimLightColor = sanitizeVector4Value(lightManager.lightingSettings_.rimLightColor, LightManager::LightingSettingsGPU{}.rimLightColor);
		lightManager.lightingSettings_.shadingMode = std::clamp(lightManager.lightingSettings_.shadingMode, 0u, 2u);
		lightManager.lightingSettings_.enableIBL = lightManager.lightingSettings_.enableIBL != 0u ? 1u : 0u;
		lightManager.lightingSettings_.iblDiffuseStrength = clampFiniteValue(lightManager.lightingSettings_.iblDiffuseStrength, 0.0f, 0.0f, 8.0f);
		lightManager.lightingSettings_.iblSpecularStrength = clampFiniteValue(lightManager.lightingSettings_.iblSpecularStrength, 0.0f, 0.0f, 8.0f);
		lightManager.shadowBias_ = clampFiniteValue(lightManager.shadowBias_, 0.0f, 0.0f, 0.1f);
		lightManager.normalBias_ = clampFiniteValue(lightManager.normalBias_, 0.025f, 0.0f, 1.0f);
		lightManager.shadowStrength_ = clampFiniteValue(lightManager.shadowStrength_, 0.6f, 0.0f, 1.0f);
		const uint32_t sanitizedShadowMapSize = normalizeShadowMapSizeValue(static_cast<int32_t>(lightManager.shadowMapSize_));
		lightManager.directionalShadowWidth_ = clampFiniteValue(lightManager.directionalShadowWidth_, 35.0f, 1.0f, 5000.0f);
		lightManager.directionalShadowHeight_ = clampFiniteValue(lightManager.directionalShadowHeight_, 35.0f, 1.0f, 5000.0f);
		lightManager.directionalShadowNearZ_ = clampFiniteValue(lightManager.directionalShadowNearZ_, 0.1f, 0.001f, 5000.0f);
		lightManager.directionalShadowFarZ_ = clampFiniteValue(lightManager.directionalShadowFarZ_, 120.0f, lightManager.directionalShadowNearZ_ + 0.001f, 20000.0f);
		lightManager.shadowFocusMode_ = static_cast<LightManager::ShadowFocusMode>(std::clamp(static_cast<int32_t>(lightManager.shadowFocusMode_), 0, 3));
		light.direction = Vector3::NormalizeSafe(light.direction, { 0.0f, -1.0f, 0.0f });
		light.color = sanitizeVector4Value(light.color, { 1.0f, 1.0f, 1.0f, 1.0f });
		light.intensity = clampFiniteValue(light.intensity, 1.0f, 0.0f, 100.0f);
		if (lightManager.dxCommon_ && lightManager.shadowMapSize_ != sanitizedShadowMapSize)
		{
			lightManager.ApplyShadowMapSizeFromParameter(sanitizedShadowMapSize); // Legacy/CSM/Pointの希望解像度を同時に更新する。
		}
		else
		{
			lightManager.shadowMapSize_ = sanitizedShadowMapSize;
		}

		lightManager.lightParameterController_.SyncFromCurrentState(); // 既存プリセット適用後もParameters側の表示と保存値候補を最新化する。
		return true;
	}
}
