#include "DataAssetPresets.h"

#include "SkyBox.h"
#include "Sprite.h"

#include <algorithm>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		nlohmann::json ToJ(const Vector2& v) { return { v.x, v.y }; }
		nlohmann::json ToJ(const Vector3& v) { return { v.x, v.y, v.z }; }
		nlohmann::json ToJ(const Vector4& v) { return { v.x, v.y, v.z, v.w }; }
		void FromJ(const nlohmann::json& j, Vector2& v) { if (j.is_array() && j.size() >= 2) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); } }
		void FromJ(const nlohmann::json& j, Vector3& v) { if (j.is_array() && j.size() >= 3) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); } }
		void FromJ(const nlohmann::json& j, Vector4& v) { if (j.is_array() && j.size() >= 4) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); v.w = j[3].get<float>(); } }
	}
#define READ_NUM(name) if(inJson.contains(#name)){ name = inJson[#name].get<decltype(name)>(); }
	void CloudLayerPreset::ToJson(nlohmann::json& outJson) const
	{
		outJson = {
			{"enabled",enabled}, {"texturePath",texturePath}, {"height",height}, {"scale",scale},
			{"scrollSpeed",ToJ(scrollSpeed)}, {"uvOffset",ToJ(uvOffset)}, {"alpha",alpha}, {"tintColor",ToJ(tintColor)}
		};
	}

	void CloudLayerPreset::FromJson(const nlohmann::json& inJson)
	{
		READ_NUM(enabled);
		if (inJson.contains("texturePath")) texturePath = inJson["texturePath"].get<std::string>();
		READ_NUM(height); READ_NUM(scale); READ_NUM(alpha);
		FromJ(inJson.value("scrollSpeed", nlohmann::json::array()), scrollSpeed);
		FromJ(inJson.value("uvOffset", nlohmann::json::array()), uvOffset);
		FromJ(inJson.value("tintColor", nlohmann::json::array()), tintColor);
	}

	void SkyBoxPreset::ToJson(nlohmann::json& outJson) const
	{
		nlohmann::json cloudJson;
		cloud.ToJson(cloudJson);
		outJson = {
			{"name",name}, {"enabled",enabled}, {"skyType",skyType}, {"texturePath",texturePath},
			{"topColor",ToJ(topColor)}, {"bottomColor",ToJ(bottomColor)}, {"horizonColor",ToJ(horizonColor)},
			{"rotation",ToJ(rotation)}, {"scale",ToJ(scale)}, {"brightness",brightness},
			{"tintColor",ToJ(tintColor)}, {"cloud",cloudJson}
		};
	}

	void SkyBoxPreset::FromJson(const nlohmann::json& inJson)
	{
		if (inJson.contains("name")) name = inJson["name"].get<std::string>();
		READ_NUM(enabled);
		if (inJson.contains("skyType")) skyType = inJson["skyType"].get<std::string>();
		if (inJson.contains("texturePath")) texturePath = inJson["texturePath"].get<std::string>();
		FromJ(inJson.value("topColor", nlohmann::json::array()), topColor);
		FromJ(inJson.value("bottomColor", nlohmann::json::array()), bottomColor);
		FromJ(inJson.value("horizonColor", nlohmann::json::array()), horizonColor);
		FromJ(inJson.value("rotation", nlohmann::json::array()), rotation);
		FromJ(inJson.value("scale", nlohmann::json::array()), scale);
		READ_NUM(brightness);
		FromJ(inJson.value("tintColor", nlohmann::json::array()), tintColor);
		if (inJson.contains("cloud") && inJson["cloud"].is_object()) cloud.FromJson(inJson["cloud"]);
	}

	SkyBoxPreset* SkyBoxPresetCollection::FindActivePreset()
	{
		auto it = std::find_if(presets.begin(), presets.end(), [this](const SkyBoxPreset& preset) { return preset.name == activePresetName; });
		return it != presets.end() ? &(*it) : nullptr;
	}

	const SkyBoxPreset* SkyBoxPresetCollection::FindActivePreset() const
	{
		auto it = std::find_if(presets.begin(), presets.end(), [this](const SkyBoxPreset& preset) { return preset.name == activePresetName; });
		return it != presets.end() ? &(*it) : nullptr;
	}

	void SkyBoxPresetCollection::ToJson(nlohmann::json& outJson) const
	{
		outJson = nlohmann::json::object();
		outJson["activePresetName"] = activePresetName;
		outJson["presets"] = nlohmann::json::array();
		for (const SkyBoxPreset& preset : presets)
		{
			nlohmann::json presetJson;
			preset.ToJson(presetJson);
			outJson["presets"].push_back(presetJson);
		}
	}

	void SkyBoxPresetCollection::FromJson(const nlohmann::json& inJson)
	{
		if (inJson.contains("activePresetName")) activePresetName = inJson["activePresetName"].get<std::string>();
		if (inJson.contains("presets") && inJson["presets"].is_array())
		{
			std::vector<SkyBoxPreset> loadedPresets;
			for (const auto& presetJson : inJson["presets"])
			{
				SkyBoxPreset preset;
				preset.FromJson(presetJson);
				loadedPresets.push_back(preset);
			}
			if (!loadedPresets.empty()) presets = std::move(loadedPresets);
		}
		if (!FindActivePreset() && !presets.empty()) activePresetName = presets.front().name;
	}

	void ApplySkyBoxPreset(SkyBox& skyBox, const SkyBoxPreset& preset, bool reloadTexture)
	{
		skyBox.SetSkyType(preset.skyType);
		skyBox.SetTexture(preset.texturePath, reloadTexture);
		skyBox.GetWorldTransform().rotate_ = preset.rotation;
		skyBox.GetWorldTransform().scale_ = preset.scale;
		skyBox.SetGradientColors(preset.topColor, preset.bottomColor, preset.horizonColor);
		skyBox.SetCloudLayer(preset.cloud.enabled, preset.cloud.texturePath, preset.cloud.height, preset.cloud.scale,
			preset.cloud.scrollSpeed, preset.cloud.uvOffset, preset.cloud.alpha, preset.cloud.tintColor, reloadTexture);
		skyBox.SetColor({
			preset.tintColor.x * preset.brightness,
			preset.tintColor.y * preset.brightness,
			preset.tintColor.z * preset.brightness,
			preset.tintColor.w
		});
	}
	void LightPreset::ToJson(nlohmann::json& outJson) const
	{
		outJson = {
			{"directionalDirection",ToJ(directionalDirection)},
			{"color",ToJ(color)},
			{"intensity",intensity},
			{"enableShadow",enableShadow},
			{"shadowBias",shadowBias},
			{"normalBias",normalBias},
			{"shadowStrength",shadowStrength},
			{"shadowMapSize",shadowMapSize},
			{"shadowWidth",shadowWidth},
			{"shadowHeight",shadowHeight},
			{"shadowNearZ",shadowNearZ},
			{"shadowFarZ",shadowFarZ},
			{"shadowFocusMode",shadowFocusMode},
			{"ambientColor",ToJ(ambientColor)},
			{"fogColor",ToJ(fogColor)},
			{"exposure",exposure},
			{"contrast",contrast},
			{"fogStart",fogStart},
			{"fogEnd",fogEnd},
			{"enableFog",enableFog},
			{"specularStrength",specularStrength},
			{"diffuseStrength",diffuseStrength},
			{"specularPowerScale",specularPowerScale},
			{"rimLightStrength",rimLightStrength},
			{"rimLightPower",rimLightPower},
			{"enableRimLight",enableRimLight},
			{"enableHalfLambert",enableHalfLambert},
			{"rimLightColor",ToJ(rimLightColor)},
			{"shadingMode",shadingMode},
			{"enableIBL",enableIBL},
			{"iblDiffuseStrength",iblDiffuseStrength},
			{"iblSpecularStrength",iblSpecularStrength}
		};
	}

	void LightPreset::FromJson(const nlohmann::json& inJson)
	{
		FromJ(inJson.value("directionalDirection", nlohmann::json::array()), directionalDirection);
		FromJ(inJson.value("color", nlohmann::json::array()), color);
		READ_NUM(intensity);
		READ_NUM(enableShadow);
		READ_NUM(shadowBias);
		READ_NUM(normalBias);
		READ_NUM(shadowStrength);
		READ_NUM(shadowMapSize);
		READ_NUM(shadowWidth);
		READ_NUM(shadowHeight);
		READ_NUM(shadowNearZ);
		READ_NUM(shadowFarZ);
		READ_NUM(shadowFocusMode);
		FromJ(inJson.value("ambientColor", nlohmann::json::array()), ambientColor);
		FromJ(inJson.value("fogColor", nlohmann::json::array()), fogColor);
		READ_NUM(exposure);
		READ_NUM(contrast);
		READ_NUM(fogStart);
		READ_NUM(fogEnd);
		READ_NUM(enableFog);
		READ_NUM(specularStrength);
		READ_NUM(diffuseStrength);
		READ_NUM(specularPowerScale);
		READ_NUM(rimLightStrength);
		READ_NUM(rimLightPower);
		READ_NUM(enableRimLight);
		READ_NUM(enableHalfLambert);
		FromJ(inJson.value("rimLightColor", nlohmann::json::array()), rimLightColor);
		READ_NUM(shadingMode);
		// IBL設定は旧LightPresetに存在しないため、欠損時は初期OFF/0強度へfallbackして既存プリセット互換を守る。
		READ_NUM(enableIBL);
		READ_NUM(iblDiffuseStrength);
		READ_NUM(iblSpecularStrength);
	}
	void PostEffectPreset::ToJson(nlohmann::json& outJson) const { outJson = {{"enabled",enabled},{"activeEffect",activeEffect},{"bloomIntensity",bloomIntensity},{"vignetteIntensity",vignetteIntensity},{"grayscale",grayscale},{"sepia",sepia},{"fade",fade}}; }
	void PostEffectPreset::FromJson(const nlohmann::json& inJson) { READ_NUM(enabled); if(inJson.contains("activeEffect")) activeEffect=inJson["activeEffect"].get<std::string>(); READ_NUM(bloomIntensity); READ_NUM(vignetteIntensity); READ_NUM(grayscale); READ_NUM(sepia); READ_NUM(fade); }
	void Object3DPreset::ToJson(nlohmann::json& outJson) const { outJson={{"modelPath",modelPath},{"texturePath",texturePath},{"position",ToJ(position)},{"rotation",ToJ(rotation)},{"scale",ToJ(scale)},{"visible",visible},{"castShadow",castShadow},{"receiveShadow",receiveShadow}}; }
	void Object3DPreset::FromJson(const nlohmann::json& inJson) { if(inJson.contains("modelPath"))modelPath=inJson["modelPath"].get<std::string>(); if(inJson.contains("texturePath"))texturePath=inJson["texturePath"].get<std::string>(); FromJ(inJson.value("position",nlohmann::json::array()),position); FromJ(inJson.value("rotation",nlohmann::json::array()),rotation); FromJ(inJson.value("scale",nlohmann::json::array()),scale); READ_NUM(visible); READ_NUM(castShadow); READ_NUM(receiveShadow); }
	void SpritePreset::ToJson(nlohmann::json& outJson) const { outJson={{"texturePath",texturePath},{"position",ToJ(position)},{"size",ToJ(size)},{"rotation",rotation},{"anchor",ToJ(anchor)},{"color",ToJ(color)},{"visible",visible},{"layer",layer},{"pivot",ToJ(pivot)},{"textureLeftTop",ToJ(textureLeftTop)},{"textureSize",ToJ(textureSize)},{"enableAlpha",enableAlpha},{"drawOrder",drawOrder}}; }
	void SpritePreset::FromJson(const nlohmann::json& inJson) { if(inJson.contains("texturePath"))texturePath=inJson["texturePath"].get<std::string>(); FromJ(inJson.value("position",nlohmann::json::array()),position); FromJ(inJson.value("size",nlohmann::json::array()),size); READ_NUM(rotation); FromJ(inJson.value("anchor",nlohmann::json::array()),anchor); FromJ(inJson.value("color",nlohmann::json::array()),color); READ_NUM(visible); READ_NUM(layer); FromJ(inJson.value("pivot",nlohmann::json::array()),pivot); FromJ(inJson.value("textureLeftTop",nlohmann::json::array()),textureLeftTop); FromJ(inJson.value("textureSize",nlohmann::json::array()),textureSize); if(inJson.contains("uvPosition")){ FromJ(inJson.value("uvPosition",nlohmann::json::array()),textureLeftTop); } if(inJson.contains("uvSize")){ FromJ(inJson.value("uvSize",nlohmann::json::array()),textureSize); } READ_NUM(enableAlpha); READ_NUM(drawOrder); }

	void ApplySpritePreset(Sprite& sprite, const SpritePreset& preset)
	{
		if (!preset.texturePath.empty()) { sprite.SetTexture(preset.texturePath); }
		sprite.SetPosition(preset.position);
		sprite.SetSize(preset.size);
		sprite.SetRotation(preset.rotation);
		sprite.SetAnchorPoint(preset.anchor);
		sprite.SetColor(preset.color);
		const Vector2 textureSize = ((preset.textureSize.x <= 0.0f || preset.textureSize.y <= 0.0f) ? sprite.GetTextureSize() : preset.textureSize);
		sprite.SetUVRect(preset.textureLeftTop, textureSize);
		if (!preset.visible)
		{
			Vector4 hidden = preset.color;
			hidden.w = 0.0f;
			sprite.SetColor(hidden);
		}
	}
	void ParticlePreset::ToJson(nlohmann::json& outJson) const { outJson={{"emitterType",emitterType},{"position",ToJ(position)},{"spawnRate",spawnRate},{"lifetime",lifetime},{"speed",speed},{"color",ToJ(color)},{"size",size},{"loop",loop}}; }
	void ParticlePreset::FromJson(const nlohmann::json& inJson) { if(inJson.contains("emitterType"))emitterType=inJson["emitterType"].get<std::string>(); FromJ(inJson.value("position",nlohmann::json::array()),position); READ_NUM(spawnRate); READ_NUM(lifetime); READ_NUM(speed); FromJ(inJson.value("color",nlohmann::json::array()),color); READ_NUM(size); READ_NUM(loop); }
}
