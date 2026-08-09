#define NOMINMAX
#include "SpriteComponent.h"

#include "AssetPathSelector.h"
#include "Sprite.h"
#include "SpriteManager.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		Vector2 ReadVector2FromJson(const nlohmann::json& json, const char* key, const Vector2& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 2)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>()
			};
		}

		Vector4 ReadVector4FromJson(const nlohmann::json& json, const char* key, const Vector4& defaultValue)
		{
			if (!json.contains(key) || !json[key].is_array() || json[key].size() != 4)
			{
				return defaultValue; // 指定したキーが存在しない場合はデフォルト値を返す
			}

			return {
				json[key][0].get<float>(),
				json[key][1].get<float>(),
				json[key][2].get<float>(),
				json[key][3].get<float>()
			};
		}
	}

	SpriteComponent::~SpriteComponent() = default;

	void SpriteComponent::Initialize()
	{
		EnsureSprite();
		ApplySpriteSettings();
	}

	void SpriteComponent::Update([[maybe_unused]] float deltaTime)
	{
		EnsureSprite();
		ApplySpriteSettings();
	}

	void SpriteComponent::Draw()
	{
		// Screen Space SpriteはActorWorldの2D描画パスでまとめて描画する
	}

	void SpriteComponent::DrawScreenSpace()
	{
		if (!visible_ || texturePath_.empty())
		{
			return; // 非表示またはTexture未設定の場合は描画しない
		}

		if (!sprite_)
		{
			return; // Spriteが生成できない場合は描画しない
		}

		ApplySpriteSettings();
		SpriteManager::GetInstance()->SetRenderSetting_UI();
		sprite_->Draw();
	}

	bool SpriteComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_ && !texturePath_.empty();
	}

	void SpriteComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("スプライトコンポーネント");

		ComponentPropertyUtility::DrawImGui(CreateProperties());

		std::string selectedTexturePath = texturePath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##SpriteComponentTexturePath", selectedTexturePath, AssetType::Texture))
		{
			SetTexturePath(selectedTexturePath);
		}
#endif // USE_IMGUI
	}

	void SpriteComponent::Finalize()
	{
		if (sprite_)
		{
			sprite_->Finalize(); // 描画登録を解除し、Sprite本体の破棄はComponent破棄時のRAIIへ任せる。
		}

		loadedTexturePath_.clear();
	}

	void SpriteComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // ActorComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // SpriteComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<SpriteComponent*>(this)->CreateProperties(), outJson);
	}

	void SpriteComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // ActorComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);

		ApplySpriteSettings();
	}

	void SpriteComponent::SetTexturePath(const std::string& texturePath)
	{
		if (texturePath_ == texturePath)
		{
			return; // 同じTextureなら再生成しない
		}

		texturePath_ = texturePath;
		if (sprite_)
		{
			sprite_->Finalize();
			sprite_.reset();
			loadedTexturePath_.clear();
		}
	}

	void SpriteComponent::SetSize(const Vector2& size)
	{
		size_ = { std::max(size.x, 0.0f), std::max(size.y, 0.0f) };
	}

	void SpriteComponent::SetAnchor(const Vector2& anchor)
	{
		anchor_ = {
			std::clamp(anchor.x, 0.0f, 1.0f),
			std::clamp(anchor.y, 0.0f, 1.0f)
		};
	}

	void SpriteComponent::EnsureSprite()
	{
		if (texturePath_.empty() || (sprite_ && loadedTexturePath_ == texturePath_))
		{
			return; // Texture未設定、または既に同じTextureで生成済みの場合は何もしない
		}

		if (sprite_)
		{
			sprite_->Finalize();
			sprite_.reset();
		}

		sprite_ = std::make_unique<Sprite>();
		sprite_->Initialize(texturePath_);
		loadedTexturePath_ = texturePath_;
	}

	void SpriteComponent::ApplySpriteSettings()
	{
		if (!sprite_)
		{
			return; // Sprite未生成の場合は反映しない
		}

		sprite_->SetPosition(position_);
		sprite_->SetSize(size_);
		sprite_->SetColor(color_);
		sprite_->SetRotation(rotation_);
		sprite_->SetAnchorPoint(anchor_);
		sprite_->Update();
	}

	std::vector<ComponentProperty> SpriteComponent::CreateProperties()
	{
		return {
			{ "TexturePath", "テクスチャパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return texturePath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetTexturePath(*typedValue); } } },
			{ "Position", "位置", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return position_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetPosition(*typedValue); } }, 0.0f, 0.0f, 1.0f },
			{ "Size", "サイズ", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return size_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetSize(*typedValue); } }, 0.0f, 4096.0f, 1.0f, true },
			{ "Color", "色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return color_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } },
			{ "Rotation", "回転", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return rotation_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetRotation(*typedValue); } }, 0.0f, 0.0f, 0.01f },
			{ "Anchor", "アンカー", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return anchor_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetAnchor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true },
		};
	}
}
