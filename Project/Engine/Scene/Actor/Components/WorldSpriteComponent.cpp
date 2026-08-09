#define NOMINMAX
#include "WorldSpriteComponent.h"

#include "AssetPathSelector.h"
#include "CameraManager.h"
#include "GameViewportConstants.h"
#include "Matrix4x4.h"
#include "Sprite.h"
#include "SpriteManager.h"

#include <algorithm>
#include <array>
#include <cmath>
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

	WorldSpriteComponent::~WorldSpriteComponent() = default;

	void WorldSpriteComponent::Initialize()
	{
		SceneComponent::Initialize();
		EnsureSprite();
	}

	void WorldSpriteComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		EnsureSprite();
	}

	void WorldSpriteComponent::Draw()
	{
		// WorldSpriteComponentはActorWorldの2D描画パスでまとめて描画する
	}

	void WorldSpriteComponent::DrawScreenSpace()
	{
		if (!CanDrawScreenSpace())
		{
			return; // 非表示またはTexture未設定の場合は描画しない
		}

		Vector2 screenPosition{};
		if (!UpdateScreenPosition(screenPosition))
		{
			return; // カメラ背面など描画できない位置の場合は描画しない
		}

		if (!sprite_)
		{
			return; // Spriteが生成できない場合は描画しない
		}

		ApplySpriteSettings(screenPosition);
		SpriteManager::GetInstance()->SetRenderSetting_UI();
		sprite_->Draw();
	}

	bool WorldSpriteComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_ && !texturePath_.empty();
	}

	void WorldSpriteComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		SceneComponent::DrawImGui();

		ImGui::SeparatorText("ワールドスプライトコンポーネント");

		ComponentPropertyUtility::DrawImGui(CreateProperties());

		std::string selectedTexturePath = texturePath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##WorldSpriteComponentTexturePath", selectedTexturePath, AssetType::Texture))
		{
			SetTexturePath(selectedTexturePath);
		}
#endif // USE_IMGUI
	}

	void WorldSpriteComponent::Finalize()
	{
		if (sprite_)
		{
			sprite_->Finalize(); // 描画登録を解除し、Sprite本体の破棄はComponent破棄時のRAIIへ任せる。
		}

		loadedTexturePath_.clear();
	}

	void WorldSpriteComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // WorldSpriteComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<WorldSpriteComponent*>(this)->CreateProperties(), outJson);
	}

	void WorldSpriteComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	void WorldSpriteComponent::SetTexturePath(const std::string& texturePath)
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

	void WorldSpriteComponent::SetSize(const Vector2& size)
	{
		size_ = { std::max(size.x, 0.0f), std::max(size.y, 0.0f) };
	}

	bool WorldSpriteComponent::UpdateScreenPosition(Vector2& outScreenPosition) const
	{
		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Vector3 worldPosition = GetWorldPosition();

		const float clipX = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float clipY = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float clipW = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];

		if (hideWhenBehindCamera_ && clipW <= 0.0f)
		{
			return false; // カメラ背面のSpriteは描画しない
		}

		if (std::fabs(clipW) <= 0.0001f)
		{
			return false; // NDC変換できない位置は描画しない
		}

		const float ndcX = clipX / clipW;
		const float ndcY = clipY / clipW;

		const float screenWidth = static_cast<float>(GameViewportConstants::Width);
		const float screenHeight = static_cast<float>(GameViewportConstants::Height);

		outScreenPosition.x = (ndcX + 1.0f) * 0.5f * screenWidth + screenOffset_.x;
		outScreenPosition.y = (1.0f - ndcY) * 0.5f * screenHeight + screenOffset_.y;

		return true;
	}

	void WorldSpriteComponent::EnsureSprite()
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

	void WorldSpriteComponent::ApplySpriteSettings(const Vector2& screenPosition)
	{
		if (!sprite_)
		{
			return; // Sprite未生成の場合は反映しない
		}

		sprite_->SetPosition(screenPosition);
		sprite_->SetSize(size_);
		sprite_->SetColor(color_);
		sprite_->SetAnchorPoint({ 0.5f, 0.5f });
		sprite_->Update();
	}

	std::vector<ComponentProperty> WorldSpriteComponent::CreateProperties()
	{
		return {
			{ "TexturePath", "テクスチャパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return texturePath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetTexturePath(*typedValue); } } },
			{ "ScreenOffset", "スクリーンオフセット", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return screenOffset_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetScreenOffset(*typedValue); } }, 0.0f, 0.0f, 1.0f },
			{ "Size", "サイズ", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return size_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetSize(*typedValue); } }, 0.0f, 4096.0f, 1.0f, true },
			{ "Color", "色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return color_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } },
			{ "HideWhenBehindCamera", "カメラ背面で非表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return hideWhenBehindCamera_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetHideWhenBehindCamera(*typedValue); } } },
		};
	}
}
