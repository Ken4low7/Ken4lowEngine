#include "SpriteFactory.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///			SpritePreset の値を Sprite に反映する処理
	/// --------------------------------------------------------------
	std::unique_ptr<Sprite> SpriteFactory::Create(const SpritePreset& preset)
	{
		auto sprite = std::make_unique<Sprite>();

		// Sprite本体の初期化後、Presetの値をまとめて反映する。
		sprite->Initialize(preset.texturePath);
		ApplySpritePreset(*sprite, preset);
		sprite->Update();

		return sprite;
	}

	/// -------------------------------------------------------------
	///			基本項目だけで Sprite を生成する処理
	/// -------------------------------------------------------------
	std::unique_ptr<Sprite> SpriteFactory::Create(const std::string& texturePath, const Vector2& position, const Vector2& size, const Vector4& color)
	{
		SpritePreset preset{};
		preset.texturePath = texturePath;
		preset.position = position;
		preset.size = size;
		preset.anchor = { 0.5f, 0.5f };
		preset.color = color;

		return Create(preset);
	}

	/// -------------------------------------------------------------
	///			UV切り出しも含めて Sprite を生成する処理
	/// -------------------------------------------------------------
	std::unique_ptr<Sprite> SpriteFactory::CreateUv(const std::string& texturePath, const Vector2& position, const Vector2& size, const Vector2& textureLeftTop, const Vector2& textureSize, const Vector4& color)
	{
		SpritePreset preset{};
		preset.texturePath = texturePath;
		preset.position = position;
		preset.size = size;
		preset.anchor = { 0.5f, 0.5f };
		preset.color = color;
		preset.textureLeftTop = textureLeftTop;
		preset.textureSize = textureSize;

		return Create(preset);
	}
}