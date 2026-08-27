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
			case 1: return "平行光";
			case 2: return "点光源";
			case 3: return "スポットライト";
			case 4: return "矩形エリアライト";
			case 5: return "球形エリアライト";
			default: return "なし";
			}
		}

		void DrawLightDebugInfo(const LightManager::PunctualLightGPU& light)
		{
			// ライトの実GPU値を日本語項目名でまとめて確認できるようにする。
			ImGui::Text("種類: %s", LightTypeName(light.lightType));
			ImGui::Text("有効: %s", light.enabled != 0u ? "はい" : "いいえ");
			ImGui::Text("色: (%.3f, %.3f, %.3f, %.3f)", light.color.x, light.color.y, light.color.z, light.color.w);
			ImGui::Text("位置: (%.2f, %.2f, %.2f)", light.position.x, light.position.y, light.position.z);
			ImGui::Text("強度: %.2f", light.intensity);
			ImGui::Text("範囲 / 半径: %.2f / %.2f", light.distance, light.radius);
			ImGui::Text("減衰: %.2f", light.decay);
			ImGui::Text("スポット内側 / 外側 cos: %.3f / %.3f", light.cosFalloffStart, light.cosAngle);
			ImGui::Text("エリアサイズ: (%.2f, %.2f, %.2f)", light.areaSize.x, light.areaSize.y, light.areaSize.z);
			ImGui::Text("方向: (%.2f, %.2f, %.2f)", light.direction.x, light.direction.y, light.direction.z);
		}
	}

	void LightEditorPanel::Draw(LightManager& lightManager, bool* pOpen)
	{
#ifdef USE_IMGUI
		if (pOpen != nullptr && !*pOpen)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(360.0f, 480.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("ライト編集###Light Editor", pOpen))
		{
			if (ImGui::CollapsingHeader("全体ライティング / 実行時デバッグ"))
			{
				DrawPunctualLightsInspector(lightManager);
			}
			static char presetId[64] = "default_light";
			ImGui::InputText("ライトプリセットID", presetId, IM_ARRAYSIZE(presetId));
			if (ImGui::Button("現在のライト設定を保存")) { lightManager.SaveLightPreset(presetId); }
			if (ImGui::Button("選択したライトプリセットを適用")) { lightManager.ApplyLightPresetByPath(std::string("Resources/DataAssets/LightPresets/") + presetId + ".json"); }
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
		ImGui::TextUnformatted("Actorごとのライトは、詳細パネルのLightComponentから編集してください。");

		ImGui::SeparatorText("全体ライティング");
		ImGui::Text("環境光: (%.3f, %.3f, %.3f, %.3f)", lightManager.lightingSettings_.ambientColor.x, lightManager.lightingSettings_.ambientColor.y, lightManager.lightingSettings_.ambientColor.z, lightManager.lightingSettings_.ambientColor.w);
		ImGui::Text("露出 / コントラスト: %.3f / %.3f", lightManager.lightingSettings_.exposure, lightManager.lightingSettings_.contrast);
		ImGui::Text("フォグ: %s  開始 / 終了: %.2f / %.2f", lightManager.lightingSettings_.enableFog != 0u ? "有効" : "無効", lightManager.lightingSettings_.fogStart, lightManager.lightingSettings_.fogEnd);
		ImGui::Text("シェーディング方式: %u  拡散 / 鏡面: %.3f / %.3f", lightManager.lightingSettings_.shadingMode, lightManager.lightingSettings_.diffuseStrength, lightManager.lightingSettings_.specularStrength);
		ImGui::Text("リムライト: %s  強度 / 指数: %.3f / %.3f", lightManager.lightingSettings_.enableRimLight != 0u ? "有効" : "無効", lightManager.lightingSettings_.rimLightStrength, lightManager.lightingSettings_.rimLightPower);

		ImGui::SeparatorText("IBL / PBR");
		ImGui::TextUnformatted("IBLはPBRライティングで使用します。従来シェーディングは既存の環境光・直接光経路を使用します。");
		ImGui::TextDisabled("選択中のマテリアルが従来方式の場合、IBLを変更しても見た目の差が分かりにくい場合があります。");
		bool iblEnabled = lightManager.lightingSettings_.enableIBL != 0u;
		if (ImGui::Checkbox("IBLを有効化##LightEditor", &iblEnabled))
		{
			lightManager.lightingSettings_.enableIBL = iblEnabled ? 1u : 0u;
		}
		ImGui::SliderFloat("IBL拡散反射の強さ##LightEditor", &lightManager.lightingSettings_.iblDiffuseStrength, 0.0f, 2.0f);
		ImGui::SliderFloat("IBL鏡面反射の強さ##LightEditor", &lightManager.lightingSettings_.iblSpecularStrength, 0.0f, 2.0f);
		if (lightManager.lightingSettings_.enableIBL != 0u &&
			lightManager.lightingSettings_.iblDiffuseStrength <= 0.0f &&
			lightManager.lightingSettings_.iblSpecularStrength <= 0.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f), "IBLは有効ですが両方の強度が0です。効果確認のため強度を上げてください。");
		}

		ImGui::SeparatorText("全体 / 互換ライト");
		ImGui::Text("互換ライト数: %zu", lightManager.punctualLights_.size());
		if (!lightManager.punctualLights_.empty())
		{
			const auto& first = lightManager.punctualLights_.front();
			Vector3 eulerDeg = DirectionToEulerDegForLightEditor(first.direction);
			ImGui::Text("ライト #0 種類: %s", LightTypeName(first.lightType));
			ImGui::Text("ライト #0 色: (%.3f, %.3f, %.3f, %.3f)", first.color.x, first.color.y, first.color.z, first.color.w);
			ImGui::Text("ライト #0 強度: %.3f", first.intensity);
			ImGui::Text("ライト #0 ピッチ / ヨー / ロール: %.1f / %.1f / %.1f", eulerDeg.x, eulerDeg.y, eulerDeg.z);
			ImGui::Text("ライト #0 方向: (%.3f, %.3f, %.3f)", first.direction.x, first.direction.y, first.direction.z);
		}
		ImGui::TextUnformatted("互換用プリセットライトは既存データとの互換性維持のため残されています。");
		if (ImGui::Button("＋ 互換ライトを追加"))
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
		if (ImGui::Button("互換ライトをすべて削除"))
		{
			lightManager.punctualLights_.clear();
		}

		for (size_t i = 0; i < lightManager.punctualLights_.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i));
			auto& L = lightManager.punctualLights_[i];
			ImGui::Separator();
			ImGui::Text("互換ライト #%zu", i);
			DrawLightDebugInfo(L);
			ImGui::Text("エリアライト有効: %s", (L.enabled && (L.lightType == 4 || L.lightType == 5)) ? "はい" : "いいえ");
			ImGui::Text("エリアライト種類: %s", (L.lightType == 4) ? "矩形" : ((L.lightType == 5) ? "球形" : "対象外"));
			ImGui::Text("デバッグワイヤー表示: %s", ((L.enabled != 0u) && (L.lightType == 4 || L.lightType == 5)) ? "はい" : "いいえ");

			if (ImGui::Button("この互換ライトを削除"))
			{
				lightManager.punctualLights_.erase(lightManager.punctualLights_.begin() + i);
				ImGui::PopID();
				--i;
				continue;
			}
			ImGui::PopID();
		}

		ImGui::SeparatorText("LightComponentデバッグ");
		const auto& componentLights = lightManager.GetLightComponentLightsForDebug();
		ImGui::Text("コンポーネントライト数: %zu", componentLights.size());
		ImGui::TextUnformatted("読み取り専用です。値を編集する場合は対象ActorのLightComponentを選択してください。");
		for (size_t i = 0; i < componentLights.size(); ++i)
		{
			ImGui::PushID(static_cast<int>(i + 10000));
			ImGui::Separator();
			ImGui::Text("コンポーネントライト #%zu", i);
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
		ImGui::SeparatorText("シャドウ視錐台");
		ImGui::Text("シャドウ有効: %s", lightManager.enableShadow_ ? "はい" : "いいえ");
		ImGui::Text("シャドウデバッグ マップ / 係数: %s / %s", lightManager.showShadowMapDebug_ ? "表示" : "非表示", lightManager.showShadowFactorDebug_ ? "表示" : "非表示");
		ImGui::Text("シャドウ注視方式: %u", static_cast<uint32_t>(lightManager.shadowFocusMode_));
		ImGui::Text("手動シャドウ注視位置: (%.2f, %.2f, %.2f)", lightManager.manualShadowFocusPosition_.x, lightManager.manualShadowFocusPosition_.y, lightManager.manualShadowFocusPosition_.z);
		ImGui::Text("シャドウ注視オフセット: %.2f", lightManager.directionalShadowFocusOffset_);
		ImGui::Text("スポットシャドウ NearZ: %.3f", lightManager.spotShadowNearZ_);
		ImGui::Text("点光源シャドウ NearZ: %.3f", lightManager.pointShadowNearZ_);
		ImGui::Text("CSM: %s  カスケード数: 4  最大距離: %.1f  ラムダ: %.2f", lightManager.enableCsm_ ? "有効" : "無効", lightManager.csmMaxDistance_, lightManager.csmSplitLambda_);
		ImGui::TextDisabled("点光源 / CSMの値は「パラメーター > LightManager」で編集できます。");
		ImGui::Text("シャドウ担当ライト番号: %d", lightManager.shadowCasterLightIndex_);
		if (hasPointLight)
		{
			ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.45f, 1.0f), "点光源シャドウ: キューブ6面方式を使用可能");
			ImGui::Text("点光源をシャドウ担当に選択すると有効になります。");
		}
		else
		{
			ImGui::Text("点光源シャドウ: 使用可能（有効な点光源なし）");
		}
		if (hasAreaLight)
		{
			ImGui::TextColored(ImVec4(0.75f, 1.0f, 0.75f, 1.0f), "エリアライトシャドウ: 未実装");
			ImGui::Text("エリアライト方式: 近似");
		}
		for (const auto& L : lightManager.punctualLights_)
		{
			if (L.lightType == 3)
			{
				ImGui::Text("スポットコーン 外側 / 内側 cos: %.3f / %.3f", L.cosAngle, L.cosFalloffStart);
				ImGui::Text("スポット範囲: %.2f", L.distance);
				break;
			}
		}
		const LightManager::ShadowCasterType casterType = lightManager.GetActiveShadowCasterType();
		if (casterType == LightManager::ShadowCasterType::Directional)
		{
			ImGui::Text("平行光: ライトViewProjection使用中");
		}
		else if (casterType == LightManager::ShadowCasterType::Spot)
		{
			ImGui::Text("スポットライト: ライトViewProjection使用中");
			ImGui::Text("ライトViewProjection: LightManagerでスポット用に生成");
		}
		else if (casterType == LightManager::ShadowCasterType::Point)
		{
			ImGui::Text("点光源: キューブシャドウマップ6面を使用中");
		}
		else
		{
			ImGui::Text("なし: シャドウ担当ライトが選択されていません");
		}
		const char* activeCasterName = (casterType == LightManager::ShadowCasterType::Directional) ? "平行光" : (casterType == LightManager::ShadowCasterType::Spot) ? "スポットライト" : (casterType == LightManager::ShadowCasterType::Point) ? "点光源" : "なし";
		ImGui::SeparatorText("シャドウデバッグ");
		ImGui::Text("現在のシャドウ担当種類: %s", activeCasterName);
		int32_t activeLightIndex = -1;
		LightManager::PunctualLightGPU activeLight{};
		LightManager::ShadowCasterType activeType = LightManager::ShadowCasterType::None;
		const bool hasActiveLight = lightManager.TryGetActiveShadowCasterLightInfo(activeLightIndex, activeLight, activeType);
		ImGui::Text("現在のシャドウライト番号: %d", hasActiveLight ? activeLightIndex : -1);
		ImGui::Text("現在のシャドウライト方向: (%.3f, %.3f, %.3f)", hasActiveLight ? activeLight.direction.x : 0.0f, hasActiveLight ? activeLight.direction.y : 0.0f, hasActiveLight ? activeLight.direction.z : 0.0f);
		ImGui::Text("現在のシャドウライト有効: %s", (hasActiveLight && activeLight.enabled != 0u) ? "はい" : "いいえ");
		ImGui::Text("現在のシャドウライト強度: %.3f", hasActiveLight ? activeLight.intensity : 0.0f);
		ImGui::Text("シャドウ注視位置: (%.2f, %.2f, %.2f)", lightManager.currentShadowFocusPosition_.x, lightManager.currentShadowFocusPosition_.y, lightManager.currentShadowFocusPosition_.z);
		ImGui::Text("シャドウ方向: (%.3f, %.3f, %.3f)", lightManager.currentShadowDirection_.x, lightManager.currentShadowDirection_.y, lightManager.currentShadowDirection_.z);
		ImGui::Text("シャドウ距離: %.2f", lightManager.directionalShadowDistance_);
		ImGui::Text("シャドウ幅 / 高さ: %.2f / %.2f", lightManager.directionalShadowWidth_, lightManager.directionalShadowHeight_);
		ImGui::Text("シャドウ Near / Far: %.3f / %.2f", lightManager.directionalShadowNearZ_, lightManager.directionalShadowFarZ_);
		ImGui::Text("適用中シャドウ幅 / 高さ: %.2f / %.2f", lightManager.currentShadowFrustumWidth_, lightManager.currentShadowFrustumHeight_);
		ImGui::Text("適用中シャドウ Near / Far: %.3f / %.2f", lightManager.currentShadowFrustumNearZ_, lightManager.currentShadowFrustumFarZ_);
		ImGui::Text("シャドウマップサイズ: %u", lightManager.shadowMapSize_);
		ImGui::Text("シャドウバイアス / 法線バイアス: %.6f / %.4f", lightManager.shadowBias_, lightManager.normalBias_);
		ImGui::Text("有効なライト（type!=0）はGPUへ送信されます");
#else
		(void)lightManager;
#endif // USE_IMGUI
	}
}
