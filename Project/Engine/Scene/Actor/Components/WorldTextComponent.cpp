#define NOMINMAX
#include "WorldTextComponent.h"

#include "CameraManager.h"
#include "FontAtlasLoader.h"
#include "GameViewportConstants.h"
#include "Matrix4x4.h"
#include "SpriteManager.h"
#include "TextSpriteDrawer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <exception>
#include <memory>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDefaultFontBaseSize = 32.0f;

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

		TextSpriteDrawer::FontDefinition LoadFontDefinition([[maybe_unused]] const std::string& fontName)
		{
			return FontAtlasLoader::LoadFromJson(
				"UI/Font/JP/DotGothic16-Regular_atlas.dds",
				"Resources/Fonts/Compiled/JP/DotGothic16-Regular.json",
				kDefaultFontBaseSize,
				kDefaultFontBaseSize,
				U'?');
		}
	}

	WorldTextComponent::~WorldTextComponent() = default;

	void WorldTextComponent::Initialize()
	{
		SceneComponent::Initialize();
		EnsureTextDrawer();
	}

	void WorldTextComponent::Draw()
	{
		// WorldTextComponentはActorWorldの2D描画パスでまとめて描画する
	}

	void WorldTextComponent::DrawScreenSpace()
	{
		if (!CanDrawScreenSpace())
		{
			return; // 非表示または文字列が空の場合は描画しない
		}

		Vector2 screenPosition{};
		if (!UpdateScreenPosition(screenPosition))
		{
			return; // カメラ背面など描画できない位置の場合は描画しない
		}

		EnsureTextDrawer();
		if (!textDrawerReady_ || !textDrawer_)
		{
			return; // フォントが利用できない場合は描画しない
		}

		textDrawer_->Reset();
		textDrawer_->SetColor(color_);
		textDrawer_->SetScale(std::max(fontSize_, 1.0f) / kDefaultFontBaseSize);
		SpriteManager::GetInstance()->SetRenderSetting_UI();
		textDrawer_->DrawTextLeftAligned(text_, ApplyAnchor(screenPosition));
	}

	bool WorldTextComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_ && !text_.empty();
	}

	void WorldTextComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		SceneComponent::DrawImGui();

		ImGui::SeparatorText("ワールドテキストコンポーネント");

		ComponentPropertyUtility::DrawImGui(CreateProperties());
#endif // USE_IMGUI
	}

	void WorldTextComponent::Finalize()
	{
		if (textDrawer_)
		{
			textDrawer_->Finalize(); // 描画登録を解除し、TextDrawer本体の破棄はComponent破棄時のRAIIへ任せる。
		}

		loadedFontName_.clear();
		textDrawerReady_ = false;
	}

	void WorldTextComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // WorldTextComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<WorldTextComponent*>(this)->CreateProperties(), outJson);
	}

	void WorldTextComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	void WorldTextComponent::SetFontSize(float fontSize)
	{
		fontSize_ = std::clamp(fontSize, 1.0f, 256.0f);
	}

	void WorldTextComponent::SetAnchor(const Vector2& anchor)
	{
		anchor_ = {
			std::clamp(anchor.x, 0.0f, 1.0f),
			std::clamp(anchor.y, 0.0f, 1.0f)
		};
	}

	void WorldTextComponent::SetFontName(const std::string& fontName)
	{
		if (fontName_ == fontName)
		{
			return; // 同じフォント名なら再読み込みしない
		}

		fontName_ = fontName;
		if (textDrawer_)
		{
			textDrawer_->Finalize();
			textDrawer_.reset();
		}

		loadedFontName_.clear();
		textDrawerReady_ = false;
	}

	bool WorldTextComponent::UpdateScreenPosition(Vector2& outScreenPosition) const
	{
		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Vector3 worldPosition = GetWorldPosition();

		const float clipX = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float clipY = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float clipW = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];

		if (hideWhenBehindCamera_ && clipW <= 0.0f)
		{
			return false; // カメラ背面のTextは描画しない
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

	void WorldTextComponent::EnsureTextDrawer()
	{
		if (textDrawer_ && textDrawerReady_ && loadedFontName_ == fontName_)
		{
			return; // 既に同じフォントで初期化済みの場合は何もしない
		}

		textDrawer_ = std::make_unique<TextSpriteDrawer>();
		try
		{
			textDrawer_->Initialize(LoadFontDefinition(fontName_));
			loadedFontName_ = fontName_;
			textDrawerReady_ = true;
		} catch (const std::exception&)
		{
			textDrawer_->Finalize();
			textDrawerReady_ = false;
		}
	}

	Vector2 WorldTextComponent::ApplyAnchor(const Vector2& position)
	{
		if (!textDrawer_)
		{
			return position; // TextDrawerが無い場合は指定位置をそのまま使う
		}

		textDrawer_->SetScale(std::max(fontSize_, 1.0f) / kDefaultFontBaseSize);

		return {
			position.x - textDrawer_->MeasureWidth(text_) * anchor_.x,
			position.y - std::max(fontSize_, 1.0f) * anchor_.y
		};
	}

	std::vector<ComponentProperty> WorldTextComponent::CreateProperties()
	{
		return {
			{ "Text", "テキスト", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return text_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetText(*typedValue); } }, 0.0f, 0.0f, 0.1f, false, {}, ComponentPropertyDisplay::MultilineText },
			{ "FontName", "フォント名", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return fontName_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetFontName(*typedValue); } } },
			{ "ScreenOffset", "スクリーンオフセット", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return screenOffset_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetScreenOffset(*typedValue); } }, 0.0f, 0.0f, 1.0f },
			{ "FontSize", "フォントサイズ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return fontSize_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetFontSize(*typedValue); } }, 1.0f, 256.0f, 1.0f, true },
			{ "Color", "色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return color_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } },
			{ "HideWhenBehindCamera", "カメラ背面で非表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return hideWhenBehindCamera_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetHideWhenBehindCamera(*typedValue); } } },
			{ "Anchor", "アンカー", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return anchor_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetAnchor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true },
		};
	}
}
