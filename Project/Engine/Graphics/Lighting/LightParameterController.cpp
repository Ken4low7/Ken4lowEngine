#include "LightParameterController.h"

#include "LightManager.h"
#include "ParameterManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <filesystem>
#include <string>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kLightManagerGroup = "LightManager";

		float ClampFinite(float value, float fallback, float minValue, float maxValue)
		{
			if (!std::isfinite(value))
			{
				return fallback;
			}
			return std::clamp(value, minValue, maxValue);
		}

		Vector3 ClampFiniteVector3(const Vector3& value, const Vector3& fallback, const Vector3& minValue, const Vector3& maxValue)
		{
			return {
				ClampFinite(value.x, fallback.x, minValue.x, maxValue.x),
				ClampFinite(value.y, fallback.y, minValue.y, maxValue.y),
				ClampFinite(value.z, fallback.z, minValue.z, maxValue.z)
			};
		}

		Vector4 SanitizeVector4(const Vector4& value, const Vector4& fallback)
		{
			return {
				std::isfinite(value.x) ? value.x : fallback.x,
				std::isfinite(value.y) ? value.y : fallback.y,
				std::isfinite(value.z) ? value.z : fallback.z,
				std::isfinite(value.w) ? value.w : fallback.w
			};
		}

		template<typename T>
		T GetLightParameterOrDefault(ParameterManager* parameters, const std::string& key, const T& fallback)
		{
			try
			{
				return parameters->GetValue<T>(kLightManagerGroup, key);
			}
			catch (const std::exception&)
			{
				return fallback;
			}
		}

		uint32_t NormalizeShadowMapSize(int32_t value)
		{
			constexpr std::array<int32_t, 4> kSafeSizes = { 512, 1024, 2048, 4096 };
			value = std::clamp(value, kSafeSizes.front(), kSafeSizes.back());
			int32_t bestSize = kSafeSizes.front();
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
		}
	}

	void LightParameterController::Initialize(LightManager* lightManager)
	{
		lightManager_ = lightManager;
		registered_ = false;
	}

	void LightParameterController::Finalize()
	{
		if (registered_)
		{
			// Finalize後に破棄済みLightManagerへ反映処理が飛ばないよう、ParameterManagerとの接続を解除する。
			ParameterManager::GetInstance()->UnregisterParameterApplier(kLightManagerGroup, this);
			registered_ = false;
		}
		lightManager_ = nullptr;
	}

	void LightParameterController::RegisterParameters()
	{
		if (!lightManager_ || registered_)
		{
			return;
		}

		auto* parameters = ParameterManager::GetInstance();
		parameters->CreateGroup(kLightManagerGroup);
		const auto& lightingSettings = lightManager_->GetLightingSettingsForParameter();
		const auto shadowSettings = lightManager_->GetShadowSettingsForParameter();

		// LightManagerの保存対象値をParameterManagerへ集約し、通常ImGuiとの二重管理を避ける。
		parameters->AddItem(kLightManagerGroup, "ambientColor", lightingSettings.ambientColor);
		parameters->AddItem(kLightManagerGroup, "fogColor", lightingSettings.fogColor);
		parameters->AddItem(kLightManagerGroup, "exposure", lightingSettings.exposure, 0.25f, 2.0f);
		parameters->AddItem(kLightManagerGroup, "contrast", lightingSettings.contrast, 0.50f, 1.75f);
		parameters->AddItem(kLightManagerGroup, "fogStart", lightingSettings.fogStart, 0.0f, 250.0f);
		parameters->AddItem(kLightManagerGroup, "fogEnd", lightingSettings.fogEnd, 1.0f, 500.0f);
		parameters->AddItem(kLightManagerGroup, "enableFog", lightingSettings.enableFog != 0u);
		parameters->AddItem(kLightManagerGroup, "specularStrength", lightingSettings.specularStrength, 0.0f, 0.5f);
		parameters->AddItem(kLightManagerGroup, "diffuseStrength", lightingSettings.diffuseStrength, 0.0f, 3.0f);
		parameters->AddItem(kLightManagerGroup, "specularPowerScale", lightingSettings.specularPowerScale, 0.05f, 4.0f);
		parameters->AddItem(kLightManagerGroup, "rimLightStrength", lightingSettings.rimLightStrength, 0.0f, 2.0f);
		parameters->AddItem(kLightManagerGroup, "rimLightPower", lightingSettings.rimLightPower, 0.1f, 8.0f);
		parameters->AddItem(kLightManagerGroup, "enableRimLight", lightingSettings.enableRimLight != 0u);
		parameters->AddItem(kLightManagerGroup, "enableHalfLambert", lightingSettings.enableHalfLambert != 0u);
		parameters->AddItem(kLightManagerGroup, "rimLightColor", lightingSettings.rimLightColor);
		parameters->AddItem(kLightManagerGroup, "shadingMode", static_cast<int32_t>(lightingSettings.shadingMode), 0, 2);
		// IBLはPBR Direct Lightingとは別に切り替え、環境リソース未設定時は初期OFF/0強度で既存見た目を保つ。
		parameters->AddItem(kLightManagerGroup, "enableIBL", lightingSettings.enableIBL != 0u);
		parameters->AddItem(kLightManagerGroup, "iblDiffuseStrength", lightingSettings.iblDiffuseStrength, 0.0f, 2.0f);
		parameters->AddItem(kLightManagerGroup, "iblSpecularStrength", lightingSettings.iblSpecularStrength, 0.0f, 2.0f);

		parameters->AddItem(kLightManagerGroup, "enableShadow", shadowSettings.enableShadow);
		parameters->AddItem(kLightManagerGroup, "shadowBias", shadowSettings.shadowBias, 0.0f, 0.01f);
		parameters->AddItem(kLightManagerGroup, "normalBias", shadowSettings.normalBias, 0.0f, 0.1f);
		parameters->AddItem(kLightManagerGroup, "shadowStrength", shadowSettings.shadowStrength, 0.0f, 1.0f);
		parameters->AddItem(kLightManagerGroup, "shadowMapSize", static_cast<int32_t>(shadowSettings.shadowMapSize), 512, 4096);
		parameters->AddItem(kLightManagerGroup, "showShadowMapDebug", shadowSettings.showShadowMapDebug);
		parameters->AddItem(kLightManagerGroup, "showShadowFactorDebug", shadowSettings.showShadowFactorDebug);
		parameters->AddItem(kLightManagerGroup, "shadowCasterLightIndex", shadowSettings.shadowCasterLightIndex, -1, 31);
		parameters->AddItem(kLightManagerGroup, "shadowFocusMode", static_cast<int32_t>(shadowSettings.shadowFocusMode), 0, 3);
		parameters->AddItem(kLightManagerGroup, "manualShadowFocusPosition", shadowSettings.manualShadowFocusPosition, Vector3{ -500.0f, -500.0f, -500.0f }, Vector3{ 500.0f, 500.0f, 500.0f });
		parameters->AddItem(kLightManagerGroup, "directionalShadowDistance", shadowSettings.directionalShadowDistance, 1.0f, 500.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowWidth", shadowSettings.directionalShadowWidth, 5.0f, 300.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowHeight", shadowSettings.directionalShadowHeight, 5.0f, 300.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowNearZ", shadowSettings.directionalShadowNearZ, 0.01f, 50.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowFarZ", shadowSettings.directionalShadowFarZ, 1.01f, 1000.0f);
		parameters->AddItem(kLightManagerGroup, "directionalShadowFocusOffset", shadowSettings.directionalShadowFocusOffset, -200.0f, 200.0f);
		parameters->AddItem(kLightManagerGroup, "spotShadowNearZ", shadowSettings.spotShadowNearZ, 0.01f, 50.0f);
		parameters->AddItem(kLightManagerGroup, "pointShadowNearZ", shadowSettings.pointShadowNearZ, 0.01f, 50.0f);
		parameters->AddItem(kLightManagerGroup, "enableCsm", shadowSettings.enableCsm);
		parameters->AddItem(kLightManagerGroup, "csmMaxDistance", shadowSettings.csmMaxDistance, 10.0f, 1000.0f);
		parameters->AddItem(kLightManagerGroup, "csmSplitLambda", shadowSettings.csmSplitLambda, 0.0f, 1.0f);

		// Light #0が存在しない場合は、保存値の反映先として既定ライトを補う。
		lightManager_->EnsureDefaultLightForParameter();
		const auto& light = lightManager_->GetPunctualLightsForParameter().front();
		parameters->AddItem(kLightManagerGroup, "light0.lightType", static_cast<int32_t>(light.lightType), 0, 5);
		parameters->AddItem(kLightManagerGroup, "light0.enabled", light.enabled != 0u);
		parameters->AddItem(kLightManagerGroup, "light0.color", light.color);
		parameters->AddItem(kLightManagerGroup, "light0.intensity", light.intensity, 0.0f, 20.0f);
		parameters->AddItem(kLightManagerGroup, "light0.direction", light.direction, Vector3{ -1.0f, -1.0f, -1.0f }, Vector3{ 1.0f, 1.0f, 1.0f });
		parameters->AddItem(kLightManagerGroup, "light0.position", light.position, Vector3{ -50.0f, -50.0f, -50.0f }, Vector3{ 50.0f, 50.0f, 50.0f });
		parameters->AddItem(kLightManagerGroup, "light0.radius", light.radius, 0.0f, 200.0f);
		parameters->AddItem(kLightManagerGroup, "light0.decay", light.decay, 0.0f, 10.0f);
		parameters->AddItem(kLightManagerGroup, "light0.distance", light.distance, 0.0f, 200.0f);
		parameters->AddItem(kLightManagerGroup, "light0.cosFalloffStart", light.cosFalloffStart, 0.0f, 1.0f);
		parameters->AddItem(kLightManagerGroup, "light0.cosAngle", light.cosAngle, 0.0f, 1.0f);
		parameters->AddItem(kLightManagerGroup, "light0.areaSize", light.areaSize, Vector3{ 0.0f, 0.0f, 0.0f }, Vector3{ 50.0f, 50.0f, 50.0f });

		if (std::filesystem::exists("Resources/ParameterManager/LightManager.json"))
		{
			// 既存のLightManager.jsonを読み込み、保存済みキー名との互換性を維持する。
			parameters->LoadFile(kLightManagerGroup);
		}
		// Parametersウィンドウの保存/読み込み/反映ボタンを、LightManagerへの再反映処理へ接続する。
		parameters->RegisterParameterApplier(kLightManagerGroup, this, [this]() { ApplyParameters(); });
		registered_ = true;
	}

	void LightParameterController::ApplyParameters()
	{
		if (!lightManager_)
		{
			return;
		}

		auto* parameters = ParameterManager::GetInstance();
		auto& settings = lightManager_->GetMutableLightingSettingsForParameter();
		auto shadowSettings = lightManager_->GetShadowSettingsForParameter();

		// 保存値をLightManagerのLighting設定へ反映する前に、不正な数値を安全範囲へ補正する。
		settings.ambientColor = SanitizeVector4(GetLightParameterOrDefault(parameters, "ambientColor", settings.ambientColor), LightManager::LightingSettingsGPU{}.ambientColor);
		settings.fogColor = SanitizeVector4(GetLightParameterOrDefault(parameters, "fogColor", settings.fogColor), LightManager::LightingSettingsGPU{}.fogColor);
		settings.exposure = ClampFinite(GetLightParameterOrDefault(parameters, "exposure", settings.exposure), 1.0f, 0.05f, 8.0f);
		settings.contrast = ClampFinite(GetLightParameterOrDefault(parameters, "contrast", settings.contrast), 1.0f, 0.05f, 4.0f);
		settings.fogStart = ClampFinite(GetLightParameterOrDefault(parameters, "fogStart", settings.fogStart), 45.0f, 0.0f, 10000.0f);
		settings.fogEnd = ClampFinite(GetLightParameterOrDefault(parameters, "fogEnd", settings.fogEnd), 140.0f, settings.fogStart + 1.0f, 20000.0f);
		settings.enableFog = GetLightParameterOrDefault(parameters, "enableFog", settings.enableFog != 0u) ? 1u : 0u;
		settings.specularStrength = ClampFinite(GetLightParameterOrDefault(parameters, "specularStrength", settings.specularStrength), 0.08f, 0.0f, 4.0f);
		settings.diffuseStrength = ClampFinite(GetLightParameterOrDefault(parameters, "diffuseStrength", settings.diffuseStrength), 1.0f, 0.0f, 8.0f);
		settings.specularPowerScale = ClampFinite(GetLightParameterOrDefault(parameters, "specularPowerScale", settings.specularPowerScale), 1.0f, 0.01f, 16.0f);
		settings.rimLightStrength = ClampFinite(GetLightParameterOrDefault(parameters, "rimLightStrength", settings.rimLightStrength), 0.0f, 0.0f, 8.0f);
		settings.rimLightPower = ClampFinite(GetLightParameterOrDefault(parameters, "rimLightPower", settings.rimLightPower), 2.0f, 0.01f, 32.0f);
		settings.enableRimLight = GetLightParameterOrDefault(parameters, "enableRimLight", settings.enableRimLight != 0u) ? 1u : 0u;
		settings.enableHalfLambert = GetLightParameterOrDefault(parameters, "enableHalfLambert", settings.enableHalfLambert != 0u) ? 1u : 0u;
		settings.rimLightColor = SanitizeVector4(GetLightParameterOrDefault(parameters, "rimLightColor", settings.rimLightColor), LightManager::LightingSettingsGPU{}.rimLightColor);
		settings.shadingMode = static_cast<uint32_t>(std::clamp(GetLightParameterOrDefault<int32_t>(parameters, "shadingMode", static_cast<int32_t>(settings.shadingMode)), 0, 2));
		settings.enableIBL = GetLightParameterOrDefault(parameters, "enableIBL", settings.enableIBL != 0u) ? 1u : 0u;
		settings.iblDiffuseStrength = ClampFinite(GetLightParameterOrDefault(parameters, "iblDiffuseStrength", settings.iblDiffuseStrength), 0.0f, 0.0f, 8.0f);
		settings.iblSpecularStrength = ClampFinite(GetLightParameterOrDefault(parameters, "iblSpecularStrength", settings.iblSpecularStrength), 0.0f, 0.0f, 8.0f);

		shadowSettings.enableShadow = GetLightParameterOrDefault(parameters, "enableShadow", shadowSettings.enableShadow);
		shadowSettings.shadowBias = ClampFinite(GetLightParameterOrDefault(parameters, "shadowBias", shadowSettings.shadowBias), 0.0f, 0.0f, 0.1f);
		shadowSettings.normalBias = ClampFinite(GetLightParameterOrDefault(parameters, "normalBias", shadowSettings.normalBias), 0.025f, 0.0f, 1.0f);
		shadowSettings.shadowStrength = ClampFinite(GetLightParameterOrDefault(parameters, "shadowStrength", shadowSettings.shadowStrength), 0.6f, 0.0f, 1.0f);
		// shadowMapSizeはD3Dリソースの再生成に関わるため、対応済みサイズへ丸めてからLightManagerへ渡す。
		shadowSettings.shadowMapSize = NormalizeShadowMapSize(GetLightParameterOrDefault<int32_t>(parameters, "shadowMapSize", static_cast<int32_t>(shadowSettings.shadowMapSize)));
		shadowSettings.showShadowMapDebug = GetLightParameterOrDefault(parameters, "showShadowMapDebug", shadowSettings.showShadowMapDebug);
		shadowSettings.showShadowFactorDebug = GetLightParameterOrDefault(parameters, "showShadowFactorDebug", shadowSettings.showShadowFactorDebug);
		shadowSettings.shadowCasterLightIndex = GetLightParameterOrDefault(parameters, "shadowCasterLightIndex", shadowSettings.shadowCasterLightIndex);
		shadowSettings.shadowFocusMode = static_cast<LightManager::ShadowFocusMode>(std::clamp(GetLightParameterOrDefault<int32_t>(parameters, "shadowFocusMode", static_cast<int32_t>(shadowSettings.shadowFocusMode)), 0, 3));
		shadowSettings.manualShadowFocusPosition = ClampFiniteVector3(GetLightParameterOrDefault(parameters, "manualShadowFocusPosition", shadowSettings.manualShadowFocusPosition), { 0.0f, 0.0f, 0.0f }, { -10000.0f, -10000.0f, -10000.0f }, { 10000.0f, 10000.0f, 10000.0f });
		shadowSettings.directionalShadowDistance = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowDistance", shadowSettings.directionalShadowDistance), 60.0f, 1.0f, 5000.0f);
		shadowSettings.directionalShadowWidth = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowWidth", shadowSettings.directionalShadowWidth), 35.0f, 1.0f, 5000.0f);
		shadowSettings.directionalShadowHeight = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowHeight", shadowSettings.directionalShadowHeight), 35.0f, 1.0f, 5000.0f);
		shadowSettings.directionalShadowNearZ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowNearZ", shadowSettings.directionalShadowNearZ), 0.1f, 0.001f, 5000.0f);
		shadowSettings.directionalShadowFarZ = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowFarZ", shadowSettings.directionalShadowFarZ), 120.0f, shadowSettings.directionalShadowNearZ + 0.001f, 20000.0f);
		shadowSettings.directionalShadowFocusOffset = ClampFinite(GetLightParameterOrDefault(parameters, "directionalShadowFocusOffset", shadowSettings.directionalShadowFocusOffset), 0.0f, -10000.0f, 10000.0f);
		shadowSettings.spotShadowNearZ = ClampFinite(GetLightParameterOrDefault(parameters, "spotShadowNearZ", shadowSettings.spotShadowNearZ), 0.1f, 0.001f, 5000.0f);
		shadowSettings.pointShadowNearZ = ClampFinite(GetLightParameterOrDefault(parameters, "pointShadowNearZ", shadowSettings.pointShadowNearZ), 0.1f, 0.001f, 5000.0f);
		shadowSettings.enableCsm = GetLightParameterOrDefault(parameters, "enableCsm", shadowSettings.enableCsm);
		shadowSettings.csmMaxDistance = ClampFinite(GetLightParameterOrDefault(parameters, "csmMaxDistance", shadowSettings.csmMaxDistance), 160.0f, 10.0f, 5000.0f);
		shadowSettings.csmSplitLambda = ClampFinite(GetLightParameterOrDefault(parameters, "csmSplitLambda", shadowSettings.csmSplitLambda), 0.7f, 0.0f, 1.0f);

		// Light #0が存在しない場合は、保存値の反映先として既定ライトを補う。
		lightManager_->EnsureDefaultLightForParameter();
		auto& light = lightManager_->GetMutablePunctualLightsForParameter().front();
		light.lightType = static_cast<uint32_t>(std::clamp(GetLightParameterOrDefault<int32_t>(parameters, "light0.lightType", static_cast<int32_t>(light.lightType)), 0, 5));
		light.enabled = GetLightParameterOrDefault(parameters, "light0.enabled", light.enabled != 0u) ? 1u : 0u;
		light.color = SanitizeVector4(GetLightParameterOrDefault(parameters, "light0.color", light.color), { 1.0f, 1.0f, 1.0f, 1.0f });
		light.intensity = ClampFinite(GetLightParameterOrDefault(parameters, "light0.intensity", light.intensity), 1.0f, 0.0f, 100.0f);
		light.direction = Vector3::NormalizeSafe(ClampFiniteVector3(GetLightParameterOrDefault(parameters, "light0.direction", light.direction), { 0.0f, -1.0f, 0.0f }, { -1.0f, -1.0f, -1.0f }, { 1.0f, 1.0f, 1.0f }), { 0.0f, -1.0f, 0.0f });
		light.position = ClampFiniteVector3(GetLightParameterOrDefault(parameters, "light0.position", light.position), { 0.0f, 0.0f, 0.0f }, { -10000.0f, -10000.0f, -10000.0f }, { 10000.0f, 10000.0f, 10000.0f });
		light.radius = ClampFinite(GetLightParameterOrDefault(parameters, "light0.radius", light.radius), 0.0f, 0.0f, 10000.0f);
		light.decay = ClampFinite(GetLightParameterOrDefault(parameters, "light0.decay", light.decay), 1.0f, 0.0f, 100.0f);
		light.distance = ClampFinite(GetLightParameterOrDefault(parameters, "light0.distance", light.distance), 10.0f, 0.0f, 10000.0f);
		light.cosFalloffStart = ClampFinite(GetLightParameterOrDefault(parameters, "light0.cosFalloffStart", light.cosFalloffStart), 1.0f, 0.0f, 1.0f);
		light.cosAngle = ClampFinite(GetLightParameterOrDefault(parameters, "light0.cosAngle", light.cosAngle), 0.5f, 0.0f, 1.0f);
		if (light.cosFalloffStart < light.cosAngle) { light.cosFalloffStart = light.cosAngle; }
		light.areaSize = ClampFiniteVector3(GetLightParameterOrDefault(parameters, "light0.areaSize", light.areaSize), { 2.0f, 2.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 10000.0f, 10000.0f, 10000.0f });
		const int32_t selectableShadowLightCount = static_cast<int32_t>(
			lightManager_->GetPunctualLightsForParameter().size() + lightManager_->GetLightComponentLightsForDebug().size());
		shadowSettings.shadowCasterLightIndex = std::clamp(shadowSettings.shadowCasterLightIndex, -1, selectableShadowLightCount - 1);
		// 検証済みのShadow設定をまとめてLightManagerへ反映する。
		lightManager_->SetShadowSettingsFromParameter(shadowSettings);
		SyncFromCurrentState(); // クランプや正規化後の値をParameterManager表示にも戻す。
	}

	void LightParameterController::SyncFromCurrentState()
	{
		if (!registered_ || !lightManager_)
		{
			return;
		}

		auto* parameters = ParameterManager::GetInstance();
		const auto& settings = lightManager_->GetLightingSettingsForParameter();
		const auto shadowSettings = lightManager_->GetShadowSettingsForParameter();
		// クランプ後やプリセット反映後の現在値を、Parametersウィンドウの表示と保存候補へ戻す。
		parameters->SetValue(kLightManagerGroup, "ambientColor", settings.ambientColor);
		parameters->SetValue(kLightManagerGroup, "fogColor", settings.fogColor);
		parameters->SetValue(kLightManagerGroup, "exposure", settings.exposure);
		parameters->SetValue(kLightManagerGroup, "contrast", settings.contrast);
		parameters->SetValue(kLightManagerGroup, "fogStart", settings.fogStart);
		parameters->SetValue(kLightManagerGroup, "fogEnd", settings.fogEnd);
		parameters->SetValue(kLightManagerGroup, "enableFog", settings.enableFog != 0u);
		parameters->SetValue(kLightManagerGroup, "specularStrength", settings.specularStrength);
		parameters->SetValue(kLightManagerGroup, "diffuseStrength", settings.diffuseStrength);
		parameters->SetValue(kLightManagerGroup, "specularPowerScale", settings.specularPowerScale);
		parameters->SetValue(kLightManagerGroup, "rimLightStrength", settings.rimLightStrength);
		parameters->SetValue(kLightManagerGroup, "rimLightPower", settings.rimLightPower);
		parameters->SetValue(kLightManagerGroup, "enableRimLight", settings.enableRimLight != 0u);
		parameters->SetValue(kLightManagerGroup, "enableHalfLambert", settings.enableHalfLambert != 0u);
		parameters->SetValue(kLightManagerGroup, "rimLightColor", settings.rimLightColor);
		parameters->SetValue(kLightManagerGroup, "shadingMode", static_cast<int32_t>(settings.shadingMode));
		parameters->SetValue(kLightManagerGroup, "enableIBL", settings.enableIBL != 0u);
		parameters->SetValue(kLightManagerGroup, "iblDiffuseStrength", settings.iblDiffuseStrength);
		parameters->SetValue(kLightManagerGroup, "iblSpecularStrength", settings.iblSpecularStrength);

		parameters->SetValue(kLightManagerGroup, "enableShadow", shadowSettings.enableShadow);
		parameters->SetValue(kLightManagerGroup, "shadowBias", shadowSettings.shadowBias);
		parameters->SetValue(kLightManagerGroup, "normalBias", shadowSettings.normalBias);
		parameters->SetValue(kLightManagerGroup, "shadowStrength", shadowSettings.shadowStrength);
		parameters->SetValue(kLightManagerGroup, "shadowMapSize", static_cast<int32_t>(shadowSettings.shadowMapSize));
		parameters->SetValue(kLightManagerGroup, "showShadowMapDebug", shadowSettings.showShadowMapDebug);
		parameters->SetValue(kLightManagerGroup, "showShadowFactorDebug", shadowSettings.showShadowFactorDebug);
		parameters->SetValue(kLightManagerGroup, "shadowCasterLightIndex", shadowSettings.shadowCasterLightIndex);
		parameters->SetValue(kLightManagerGroup, "shadowFocusMode", static_cast<int32_t>(shadowSettings.shadowFocusMode));
		parameters->SetValue(kLightManagerGroup, "manualShadowFocusPosition", shadowSettings.manualShadowFocusPosition);
		parameters->SetValue(kLightManagerGroup, "directionalShadowDistance", shadowSettings.directionalShadowDistance);
		parameters->SetValue(kLightManagerGroup, "directionalShadowWidth", shadowSettings.directionalShadowWidth);
		parameters->SetValue(kLightManagerGroup, "directionalShadowHeight", shadowSettings.directionalShadowHeight);
		parameters->SetValue(kLightManagerGroup, "directionalShadowNearZ", shadowSettings.directionalShadowNearZ);
		parameters->SetValue(kLightManagerGroup, "directionalShadowFarZ", shadowSettings.directionalShadowFarZ);
		parameters->SetValue(kLightManagerGroup, "directionalShadowFocusOffset", shadowSettings.directionalShadowFocusOffset);
		parameters->SetValue(kLightManagerGroup, "spotShadowNearZ", shadowSettings.spotShadowNearZ);
		parameters->SetValue(kLightManagerGroup, "pointShadowNearZ", shadowSettings.pointShadowNearZ);
		parameters->SetValue(kLightManagerGroup, "enableCsm", shadowSettings.enableCsm);
		parameters->SetValue(kLightManagerGroup, "csmMaxDistance", shadowSettings.csmMaxDistance);
		parameters->SetValue(kLightManagerGroup, "csmSplitLambda", shadowSettings.csmSplitLambda);

		const auto& punctualLights = lightManager_->GetPunctualLightsForParameter();
		if (punctualLights.empty())
		{
			return;
		}
		const auto& light = punctualLights.front();
		parameters->SetValue(kLightManagerGroup, "light0.lightType", static_cast<int32_t>(light.lightType));
		parameters->SetValue(kLightManagerGroup, "light0.enabled", light.enabled != 0u);
		parameters->SetValue(kLightManagerGroup, "light0.color", light.color);
		parameters->SetValue(kLightManagerGroup, "light0.intensity", light.intensity);
		parameters->SetValue(kLightManagerGroup, "light0.direction", light.direction);
		parameters->SetValue(kLightManagerGroup, "light0.position", light.position);
		parameters->SetValue(kLightManagerGroup, "light0.radius", light.radius);
		parameters->SetValue(kLightManagerGroup, "light0.decay", light.decay);
		parameters->SetValue(kLightManagerGroup, "light0.distance", light.distance);
		parameters->SetValue(kLightManagerGroup, "light0.cosFalloffStart", light.cosFalloffStart);
		parameters->SetValue(kLightManagerGroup, "light0.cosAngle", light.cosAngle);
		parameters->SetValue(kLightManagerGroup, "light0.areaSize", light.areaSize);
	}
} // namespace Ken4lowEngine
