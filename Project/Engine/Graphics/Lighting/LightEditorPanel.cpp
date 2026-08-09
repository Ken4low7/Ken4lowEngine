#define NOMINMAX
#include "LightEditorPanel.h"

#include "LightManager.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

		Vector3 DirectionToEulerDegForLightEditor(const Vector3& dir)
		{
			Vector3 n = Vector3::Normalize(dir);
			const float pitch = std::asin(-n.y);
			const float yaw = std::atan2(n.x, n.z);
			return { pitch * kRadToDeg, yaw * kRadToDeg, 0.0f };
		}

		const char* LightTypeName(uint32_t lightType)
		{
			switch (lightType)
			{
			case 1: return "Directional";
			case 2: return "Point";
			case 3: return "Spot";
			case 4: return "RectArea";
			case 5: return "SphereArea";
			default: return "None";
			}
		}

		void DrawLightDebugInfo(const LightManager::PunctualLightGPU& light)
		{
			ImGui::Text("Type: %s", LightTypeName(light.lightType));
			ImGui::Text("Enabled: %s", light.enabled != 0u ? "true" : "false");
			ImGui::Text("Color: (%.3f, %.3f, %.3f, %.3f)", light.color.x, light.color.y, light.color.z, light.color.w);
			ImGui::Text("Position: (%.2f, %.2f, %.2f)", light.position.x, light.position.y, light.position.z);
			ImGui::Text("Intensity: %.2f", light.intensity);
			ImGui::Text("Range / Radius: %.2f / %.2f", light.distance, light.radius);
			ImGui::Text("Decay: %.2f", light.decay);
			ImGui::Text("Spot cosInner / cosOuter: %.3f / %.3f", light.cosFalloffStart, light.cosAngle);
			ImGui::Text("Area Size: (%.2f, %.2f, %.2f)", light.areaSize.x, light.areaSize.y, light.areaSize.z);
			ImGui::Text("Direction: (%.2f, %.2f, %.2f)", light.direction.x, light.direction.y, light.direction.z);
		}
	}

	void LightEditorPanel::Draw(LightManager& lightManager, bool* pOpen)
	{
#ifdef USE_IMGUI
		// WindowメニューのLight Editor表示フラグが閉じている間は、Runtime側のLightManagerへUI負荷を持ち込まない。
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		// LightManagerはFacadeとして入口だけ残し、ImGuiウィンドウ構築はLightEditorPanelが担当する。
		ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Light Editor", pOpen))
		{
			if (ImGui::CollapsingHeader("Global Lighting / Runtime Debug"))
			{
				DrawPunctualLightsInspector(lightManager);
			}
			static char presetId[64] = "default_light";
			ImGui::InputText("LightPreset Id", presetId, IM_ARRAYSIZE(presetId));
			if (ImGui::Button("Save Current LightPreset")) { lightManager.SaveLightPreset(presetId); }
			if (ImGui::Button("Apply Selected LightPreset")) { lightManager.ApplyLightPresetByPath(std::string("Resources/DataAssets/LightPresets/") + presetId + ".json"); }
		}
		ImGui::End();
#else
		(void)lightManager;
		(void)pOpen;
#endif // USE_IMGUI
	}

	void LightEditorPanel::DrawPunctualLightsInspector(LightManager& lightManager)
	{
#ifdef USE_IMGUI
		// Light Editorは全体設定/Debugを担当し、Actor単位のライト編集はLightComponent Detailsへ集約する。
		ImGui::TextUnformatted("Per-actor lights are edited in Actor Details > LightComponent.");

		ImGui::SeparatorText("Global Lighting");
		ImGui::Text("Ambient: (%.3f, %.3f, %.3f, %.3f)", lightManager.lightingSettings_.ambientColor.x, lightManager.lightingSettings_.ambientColor.y, lightManager.lightingSettings_.ambientColor.z, lightManager.lightingSettings_.ambientColor.w);
		ImGui::Text("Exposure / Contrast: %.3f / %.3f", lightManager.lightingSettings_.exposure, lightManager.lightingSettings_.contrast);
		ImGui::Text("Fog: %s  Start / End: %.2f / %.2f", lightManager.lightingSettings_.enableFog != 0u ? "true" : "false", lightManager.lightingSettings_.fogStart, lightManager.lightingSettings_.fogEnd);
		ImGui::Text("Shading Mode: %u  Diffuse / Specular: %.3f / %.3f", lightManager.lightingSettings_.shadingMode, lightManager.lightingSettings_.diffuseStrength, lightManager.lightingSettings_.specularStrength);
		ImGui::Text("Rim: %s  Strength / Power: %.3f / %.3f", lightManager.lightingSettings_.enableRimLight != 0u ? "true" : "false", lightManager.lightingSettings_.rimLightStrength, lightManager.lightingSettings_.rimLightPower);

		ImGui::SeparatorText("IBL / PBR");
		ImGui::TextUnformatted("IBL is used by the PBR lighting path. Legacy shading remains on the existing ambient/direct-light path.");
		ImGui::TextDisabled("If the selected material is still Legacy, changing IBL may not be visually obvious.");
		// IBLはPBR Direct Lightingと分けてON/OFFし、環境リソース未設定時も既存Lightingを壊さないよう初期OFFにする。
		bool iblEnabled = lightManager.lightingSettings_.enableIBL != 0u;
		if (ImGui::Checkbox("Enable IBL##LightEditor", &iblEnabled))
		{
			lightManager.lightingSettings_.enableIBL = iblEnabled ? 1u : 0u;
		}
		ImGui::SliderFloat("IBL Diffuse Strength##LightEditor", &lightManager.lightingSettings_.iblDiffuseStrength, 0.0f, 2.0f);
		ImGui::SliderFloat("IBL Specular Strength##LightEditor", &lightManager.lightingSettings_.iblSpecularStrength, 0.0f, 2.0f);
		if (lightManager.lightingSettings_.enableIBL != 0u &&
			lightManager.lightingSettings_.iblDiffuseStrength <= 0.0f &&
			lightManager.lightingSettings_.iblSpecularStrength <= 0.0f)
		{
			// EnableだけONでも強度が0なら結果は変わらないため、調整時に原因を見失わないようUIで明示する。
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "IBL is enabled, but both strengths are 0. Increase them to test the fallback lighting.");
		}

		ImGui::SeparatorText("Global / Legacy Lights");
		ImGui::Text("Legacy Light Count: %zu", lightManager.punctualLights_.size());
		if (!lightManager.punctualLights_.empty())
		{
			const auto& first = lightManager.punctualLights_.front();
			Vector3 eulerDeg = DirectionToEulerDegForLightEditor(first.direction);
			ImGui::Text("Light #0 Type: %s", LightTypeName(first.lightType));
			ImGui::Text("Light #0 Color: (%.3f, %.3f, %.3f, %.3f)", first.color.x, first.color.y, first.color.z, first.color.w);
			ImGui::Text("Light #0 Intensity: %.3f", first.intensity);
			ImGui::Text("Light #0 Pitch / Yaw / Roll: %.1f / %.1f / %.1f", eulerDeg.x, eulerDeg.y, eulerDeg.z);
			ImGui::Text("Light #0 Direction: (%.3f, %.3f, %.3f)", first.direction.x, first.direction.y, first.direction.z);
		}
		ImGui::TextUnformatted("Legacy preset lights remain here for compatibility.");
		if (ImGui::Button("+ Add Legacy Light"))
		{
			LightManager::PunctualLightGPU L{};
			L.lightType = 1;
			L.color = { 1, 1, 1, 1 };
			L.intensity = 1.0f;
			L.direction = { 0, -1, 0 };
			L.areaSize = { 2.0f, 2.0f, 1.0f };
			L.distance = 10.0f;
			L.decay = 1.0f;
			L.enabled = 1u;
			lightManager.punctualLights_.push_back(L);
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear Legacy Lights"))
		{
			lightManager.punctualLights_.clear();
		}

		for (size_t i = 0; i < lightManager.punctualLights_.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			auto& L = lightManager.punctualLights_[i];

			ImGui::Separator();
			ImGui::Text("Legacy Light #%zu", i);
			DrawLightDebugInfo(L);
			ImGui::Text("AreaLight active: %s", (L.enabled && (L.lightType == 4 || L.lightType == 5)) ? "true" : "false");
			ImGui::Text("AreaLight type: %s", (L.lightType == 4) ? "RectArea" : ((L.lightType == 5) ? "SphereArea" : "N/A"));
			ImGui::Text("debug wire visible: %s", ((L.enabled != 0u) && (L.lightType == 4 || L.lightType == 5)) ? "true" : "false");

			if (ImGui::Button("Remove Legacy Light"))
			{
				lightManager.punctualLights_.erase(lightManager.punctualLights_.begin() + i);
				ImGui::PopID();
				--i;
				continue;
			}
			ImGui::PopID();
		}

		ImGui::SeparatorText("LightComponent Debug");
		const auto& componentLights = lightManager.GetLightComponentLightsForDebug();
		ImGui::Text("Component Light Count: %zu", componentLights.size());
		ImGui::TextUnformatted("Read-only. Select the Actor/LightComponent to edit these values.");
		for (size_t i = 0; i < componentLights.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i + 10000));
			ImGui::Separator();
			ImGui::Text("Component Light #%zu", i);
			DrawLightDebugInfo(componentLights[i]);
			ImGui::PopID();
		}

		ImGui::Separator();
		const bool hasPointLight =
			std::any_of(lightManager.punctualLights_.begin(), lightManager.punctualLights_.end(), [](const LightManager::PunctualLightGPU& light) { return light.lightType == 2 && light.intensity > 0.0f && light.enabled != 0u; }) ||
			std::any_of(componentLights.begin(), componentLights.end(), [](const LightManager::PunctualLightGPU& light) { return light.lightType == 2 && light.intensity > 0.0f && light.enabled != 0u; });
		const bool hasAreaLight =
			std::any_of(lightManager.punctualLights_.begin(), lightManager.punctualLights_.end(), [](const LightManager::PunctualLightGPU& light) { return (light.lightType == 4 || light.lightType == 5) && light.intensity > 0.0f && light.enabled != 0u; }) ||
			std::any_of(componentLights.begin(), componentLights.end(), [](const LightManager::PunctualLightGPU& light) { return (light.lightType == 4 || light.lightType == 5) && light.intensity > 0.0f && light.enabled != 0u; });
		ImGui::SeparatorText("Shadow Frustum");
		ImGui::Text("Shadow Enabled: %s", lightManager.enableShadow_ ? "true" : "false");
		ImGui::Text("Shadow Debug Map / Factor: %s / %s", lightManager.showShadowMapDebug_ ? "true" : "false", lightManager.showShadowFactorDebug_ ? "true" : "false");
		ImGui::Text("Shadow Focus Mode: %u", static_cast<uint32_t>(lightManager.shadowFocusMode_));
		ImGui::Text("Manual Shadow Focus Position: (%.2f, %.2f, %.2f)", lightManager.manualShadowFocusPosition_.x, lightManager.manualShadowFocusPosition_.y, lightManager.manualShadowFocusPosition_.z);
		ImGui::Text("Shadow Focus Offset: %.2f", lightManager.directionalShadowFocusOffset_);
		ImGui::Text("Spot Shadow NearZ: %.3f", lightManager.spotShadowNearZ_);
		ImGui::Text("Point Shadow NearZ: %.3f", lightManager.pointShadowNearZ_);
		ImGui::Text("CSM: %s  Cascades: 4  MaxDistance: %.1f  Lambda: %.2f", lightManager.enableCsm_ ? "Enabled" : "Disabled", lightManager.csmMaxDistance_, lightManager.csmSplitLambda_);
		ImGui::TextDisabled("Edit Point/CSM values in Parameters > LightManager.");
		ImGui::Text("Shadow Caster Light Index: %d", lightManager.shadowCasterLightIndex_);
		if (hasPointLight)
		{
			ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "PointLight Shadow: Cube 6-face path ready");
			ImGui::Text("Select a Point light as Shadow Caster to activate it.");
		}
		else
		{
			ImGui::Text("PointLight Shadow: ready (no active Point light)");
		}
		if (hasAreaLight)
		{
			ImGui::TextColored(ImVec4(0.75f, 1.0f, 0.75f, 1.0f), "AreaLight Shadow: Not Implemented");
			ImGui::Text("AreaLight Model: Approximation");
		}
		for (const auto& L : lightManager.punctualLights_)
		{
			if (L.lightType == 3)
			{
				ImGui::Text("Spot cone cosOuter/cosInner: %.3f / %.3f", L.cosAngle, L.cosFalloffStart);
				ImGui::Text("Spot range: %.2f", L.distance);
				break;
			}
		}
		const LightManager::ShadowCasterType casterType = lightManager.GetActiveShadowCasterType();
		if (casterType == LightManager::ShadowCasterType::Directional)
		{
			ImGui::Text("Directional: LightViewProjection active");
		}
		else if (casterType == LightManager::ShadowCasterType::Spot)
		{
			ImGui::Text("Spot: LightViewProjection active");
			ImGui::Text("LightViewProjection: generated in LightManager (spot)");
		}
		else if (casterType == LightManager::ShadowCasterType::Point)
		{
			ImGui::Text("Point: Cube ShadowMap 6 faces active");
		}
		else
		{
			ImGui::Text("None: no shadow-casting light selected");
		}
		const char* activeCasterName = (casterType == LightManager::ShadowCasterType::Directional) ? "Directional" : (casterType == LightManager::ShadowCasterType::Spot) ? "Spot" : (casterType == LightManager::ShadowCasterType::Point) ? "Point" : "None";
		ImGui::SeparatorText("Shadow Debug");
		ImGui::Text("Active Shadow Caster Type: %s", activeCasterName);
		int32_t activeLightIndex = -1;
		LightManager::PunctualLightGPU activeLight{};
		LightManager::ShadowCasterType activeType = LightManager::ShadowCasterType::None;
		const bool hasActiveLight = lightManager.TryGetActiveShadowCasterLightInfo(activeLightIndex, activeLight, activeType);
		ImGui::Text("Active Shadow Light Index: %d", hasActiveLight ? activeLightIndex : -1);
		ImGui::Text("Active Shadow Light Direction: (%.3f, %.3f, %.3f)", hasActiveLight ? activeLight.direction.x : 0.0f, hasActiveLight ? activeLight.direction.y : 0.0f, hasActiveLight ? activeLight.direction.z : 0.0f);
		ImGui::Text("Active Shadow Light Enabled: %s", (hasActiveLight && activeLight.enabled != 0u) ? "true" : "false");
		ImGui::Text("Active Shadow Light Intensity: %.3f", hasActiveLight ? activeLight.intensity : 0.0f);
		ImGui::Text("Shadow Focus Position: (%.2f, %.2f, %.2f)", lightManager.currentShadowFocusPosition_.x, lightManager.currentShadowFocusPosition_.y, lightManager.currentShadowFocusPosition_.z);
		ImGui::Text("Shadow Direction: (%.3f, %.3f, %.3f)", lightManager.currentShadowDirection_.x, lightManager.currentShadowDirection_.y, lightManager.currentShadowDirection_.z);
		ImGui::Text("Shadow Distance: %.2f", lightManager.directionalShadowDistance_);
		ImGui::Text("Shadow Width / Height: %.2f / %.2f", lightManager.directionalShadowWidth_, lightManager.directionalShadowHeight_);
		ImGui::Text("Shadow Near / Far: %.3f / %.2f", lightManager.directionalShadowNearZ_, lightManager.directionalShadowFarZ_);
		ImGui::Text("Applied Shadow Width / Height: %.2f / %.2f", lightManager.currentShadowFrustumWidth_, lightManager.currentShadowFrustumHeight_);
		ImGui::Text("Applied Shadow Near / Far: %.3f / %.2f", lightManager.currentShadowFrustumNearZ_, lightManager.currentShadowFrustumFarZ_);
		ImGui::Text("Shadow Map Size: %u", lightManager.shadowMapSize_);
		ImGui::Text("Shadow Bias / Normal Bias: %.6f / %.4f", lightManager.shadowBias_, lightManager.normalBias_);
		ImGui::Text("Active Lights (type!=0): will be uploaded");
#else
		(void)lightManager;
#endif // USE_IMGUI
	}
}
