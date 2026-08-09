#pragma once

#include "JsonSerializable.h"
#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class SkyBox;
	class Sprite;

	struct CloudLayerPreset
	{
		bool enabled = false;
		std::string texturePath;
		float height = 160.0f;
		float scale = 1.5f;
		Vector2 scrollSpeed = { 0.002f, 0.0005f };
		Vector2 uvOffset{};
		float alpha = 0.55f;
		Vector4 tintColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		void ToJson(nlohmann::json& outJson) const;
		void FromJson(const nlohmann::json& inJson);
	};

	struct SkyBoxPreset : public JsonSerializable
	{
		std::string name = "DefaultSky";
		bool enabled = true;
		std::string skyType = "Gradient";
		std::string texturePath;
		Vector4 topColor = { 0.20f, 0.55f, 0.95f, 1.0f };
		Vector4 bottomColor = { 0.72f, 0.88f, 1.0f, 1.0f };
		Vector4 horizonColor = { 0.92f, 0.96f, 1.0f, 1.0f };
		Vector3 rotation{};
		Vector3 scale = { 10000.0f, 10000.0f, 10000.0f };
		float brightness = 1.0f;
		Vector4 tintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		CloudLayerPreset cloud{};

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct SkyBoxPresetCollection : public JsonSerializable
	{
		std::string activePresetName = "DefaultSky";
		std::vector<SkyBoxPreset> presets = { SkyBoxPreset{} };

		SkyBoxPreset* FindActivePreset();
		const SkyBoxPreset* FindActivePreset() const;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	// SkyBox の見た目だけを共通適用し、表示可否は描画側で扱う。
	void ApplySkyBoxPreset(SkyBox& skyBox, const SkyBoxPreset& preset, bool reloadTexture = false);

	struct LightPreset : public JsonSerializable
	{
		Vector3 directionalDirection = { 0.3f, -1.0f, 0.2f };
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float intensity = 1.0f;
		bool enableShadow = true;
		float shadowBias = 0.0f;
		float normalBias = 0.025f;
		float shadowStrength = 0.6f;
		uint32_t shadowMapSize = 2048;
		float shadowWidth = 35.0f;
		float shadowHeight = 35.0f;
		float shadowNearZ = 0.1f;
		float shadowFarZ = 120.0f;
		uint32_t shadowFocusMode = 0;
		Vector4 ambientColor = { 0.10f, 0.10f, 0.10f, 0.15f };
		Vector4 fogColor = { 0.58f, 0.64f, 0.70f, 1.0f };
		float exposure = 1.0f;
		float contrast = 1.0f;
		float fogStart = 45.0f;
		float fogEnd = 140.0f;
		uint32_t enableFog = 0;
		float specularStrength = 0.08f;
		float diffuseStrength = 1.0f;
		float specularPowerScale = 1.0f;
		float rimLightStrength = 0.0f;
		float rimLightPower = 2.0f;
		uint32_t enableRimLight = 0;
		uint32_t enableHalfLambert = 0;
		Vector4 rimLightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t shadingMode = 0;
		uint32_t enableIBL = 0;
		float iblDiffuseStrength = 0.0f;
		float iblSpecularStrength = 0.0f;

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct PostEffectPreset : public JsonSerializable
	{
		bool enabled = true;
		std::string activeEffect = "None";
		float bloomIntensity = 0.0f;
		float vignetteIntensity = 0.0f;
		float grayscale = 0.0f;
		float sepia = 0.0f;
		float fade = 0.0f;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct Object3DPreset : public JsonSerializable
	{
		std::string modelPath;
		std::string texturePath;
		Vector3 position{};
		Vector3 rotation{};
		Vector3 scale = { 1.0f, 1.0f, 1.0f };
		bool visible = true;
		bool castShadow = true;
		bool receiveShadow = true;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	struct SpritePreset : public JsonSerializable
	{
		std::string texturePath;
		Vector2 position{};
		Vector2 size = { 128.0f, 128.0f };
		float rotation = 0.0f;
		Vector2 anchor = { 0.5f, 0.5f };
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool visible = true;
		int layer = 0;
		Vector2 pivot = { 0.5f, 0.5f };
		Vector2 textureLeftTop = { 0.0f, 0.0f };
		Vector2 textureSize = { 0.0f, 0.0f };
		bool enableAlpha = true;
		int drawOrder = 0;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};

	void ApplySpritePreset(Sprite& sprite, const SpritePreset& preset);

	struct ParticlePreset : public JsonSerializable
	{
		std::string emitterType = "Default";
		Vector3 position{};
		float spawnRate = 10.0f;
		float lifetime = 1.0f;
		float speed = 1.0f;
		Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float size = 1.0f;
		bool loop = true;
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;
	};
}
