#define NOMINMAX
#include "GaugeComponent.h"

#include "SpriteManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		constexpr const char* kGaugeTexturePath = "Effects/white.dds";
		constexpr float kMinMaxValue = 0.0001f;

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

	GaugeComponent::~GaugeComponent() = default;

	void GaugeComponent::Initialize()
	{
		EnsureSprites();
		NormalizeValues();
	}

	void GaugeComponent::Draw()
	{
		// GaugeComponentはActorWorldの2D描画パスでまとめて描画する
	}

	void GaugeComponent::DrawScreenSpace()
	{
		if (!CanDrawScreenSpace())
		{
			return; // 非表示または無効な状態の場合は描画しない
		}

		DrawGaugeAt(position_);
	}

	bool GaugeComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_;
	}

	void GaugeComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::SeparatorText("ゲージコンポーネント");

		ComponentPropertyUtility::DrawImGui(CreateProperties());
		NormalizeValues();
#endif // USE_IMGUI
	}

	void GaugeComponent::Finalize()
	{
		if (backgroundSprite_)
		{
			backgroundSprite_->Finalize();
		}

		if (fillSprite_)
		{
			fillSprite_->Finalize();
		}

		for (auto& borderSprite : borderSprites_)
		{
			if (borderSprite)
			{
				borderSprite->Finalize();
			}
		}
	}

	void GaugeComponent::ToJson(nlohmann::json& outJson) const
	{
		ActorComponent::ToJson(outJson); // ActorComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // GaugeComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<GaugeComponent*>(this)->CreateProperties(), outJson);
	}

	void GaugeComponent::FromJson(const nlohmann::json& inJson)
	{
		ActorComponent::FromJson(inJson); // ActorComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);

		NormalizeValues();
	}

	void GaugeComponent::SetValue(float value)
	{
		value_ = value;
		NormalizeValues();
	}

	void GaugeComponent::SetMaxValue(float maxValue)
	{
		maxValue_ = maxValue;
		NormalizeValues();
	}

	void GaugeComponent::SetSize(const Vector2& size)
	{
		size_ = size;
		NormalizeValues();
	}

	void GaugeComponent::SetBorderThickness(float thickness)
	{
		borderThickness_ = thickness;
		NormalizeValues();
	}

	void GaugeComponent::DrawGaugeAt(const Vector2& screenPosition)
	{
		NormalizeValues();
		EnsureSprites();

		if (!backgroundSprite_ || !fillSprite_)
		{
			return; // ゲージ描画用Spriteが生成できない場合は描画しない
		}

		SpriteManager::GetInstance()->SetRenderSetting_UI();
		DrawColoredRect(*backgroundSprite_, screenPosition, size_, backgroundColor_);

		Vector2 fillPosition{};
		Vector2 fillSize{};
		CalculateFillRect(fillPosition, fillSize);
		if (fillSize.x > 0.0f && fillSize.y > 0.0f)
		{
			DrawColoredRect(*fillSprite_, { screenPosition.x + fillPosition.x, screenPosition.y + fillPosition.y }, fillSize, fillColor_);
		}

		DrawBorder(screenPosition, size_);
	}

	void GaugeComponent::NormalizeValues()
	{
		maxValue_ = std::max(maxValue_, kMinMaxValue);
		value_ = std::clamp(value_, 0.0f, maxValue_);
		size_.x = std::max(size_.x, 0.0f);
		size_.y = std::max(size_.y, 0.0f);
		borderThickness_ = std::max(borderThickness_, 0.0f);
	}

	void GaugeComponent::EnsureSprites()
	{
		if (!backgroundSprite_)
		{
			backgroundSprite_ = std::make_unique<Sprite>();
			backgroundSprite_->Initialize(kGaugeTexturePath);
		}

		if (!fillSprite_)
		{
			fillSprite_ = std::make_unique<Sprite>();
			fillSprite_->Initialize(kGaugeTexturePath);
		}

		for (auto& borderSprite : borderSprites_)
		{
			if (!borderSprite)
			{
				borderSprite = std::make_unique<Sprite>();
				borderSprite->Initialize(kGaugeTexturePath);
			}
		}
	}

	void GaugeComponent::DrawColoredRect(Sprite& sprite, const Vector2& position, const Vector2& size, const Vector4& color, const Vector2& anchor)
	{
		sprite.SetPosition(position);
		sprite.SetSize(size);
		sprite.SetColor(color);
		sprite.SetAnchorPoint(anchor);
		sprite.SetRotation(0.0f);
		sprite.Update();
		sprite.Draw();
	}

	void GaugeComponent::DrawBorder(const Vector2& position, const Vector2& size)
	{
		if (borderThickness_ <= 0.0f || borderColor_.w <= 0.0f)
		{
			return; // 枠線が不要な場合は描画しない
		}

		const float thickness = std::min(borderThickness_, std::min(size.x, size.y) * 0.5f);
		if (thickness <= 0.0f)
		{
			return;
		}

		DrawColoredRect(*borderSprites_[0], position, { size.x, thickness }, borderColor_);
		DrawColoredRect(*borderSprites_[1], { position.x, position.y + size.y - thickness }, { size.x, thickness }, borderColor_);
		DrawColoredRect(*borderSprites_[2], position, { thickness, size.y }, borderColor_);
		DrawColoredRect(*borderSprites_[3], { position.x + size.x - thickness, position.y }, { thickness, size.y }, borderColor_);
	}

	void GaugeComponent::CalculateFillRect(Vector2& outPosition, Vector2& outSize) const
	{
		const float ratio = maxValue_ > kMinMaxValue ? std::clamp(value_ / maxValue_, 0.0f, 1.0f) : 0.0f;

		outPosition = { 0.0f, 0.0f };
		outSize = size_;

		switch (fillDirection_)
		{
		case FillDirection::RightToLeft:
			outSize.x = size_.x * ratio;
			outPosition.x = size_.x - outSize.x;
			break;
		case FillDirection::TopToBottom:
			outSize.y = size_.y * ratio;
			break;
		case FillDirection::BottomToTop:
			outSize.y = size_.y * ratio;
			outPosition.y = size_.y - outSize.y;
			break;
		case FillDirection::LeftToRight:
		default:
			outSize.x = size_.x * ratio;
			break;
		}
	}

	const char* GaugeComponent::FillDirectionToString(FillDirection direction)
	{
		switch (direction)
		{
		case FillDirection::RightToLeft:
			return "RightToLeft";
		case FillDirection::TopToBottom:
			return "TopToBottom";
		case FillDirection::BottomToTop:
			return "BottomToTop";
		case FillDirection::LeftToRight:
		default:
			return "LeftToRight";
		}
	}

	GaugeComponent::FillDirection GaugeComponent::FillDirectionFromString(const std::string& value, FillDirection defaultValue)
	{
		if (value == "LeftToRight")
		{
			return FillDirection::LeftToRight;
		}
		if (value == "RightToLeft")
		{
			return FillDirection::RightToLeft;
		}
		if (value == "TopToBottom")
		{
			return FillDirection::TopToBottom;
		}
		if (value == "BottomToTop")
		{
			return FillDirection::BottomToTop;
		}

		return defaultValue;
	}

	std::vector<ComponentProperty> GaugeComponent::CreateProperties()
	{
		return {
			{ "MaxValue", "最大値", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return maxValue_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMaxValue(*typedValue); } }, kMinMaxValue, 1000000.0f, 1.0f, true },
			{ "Value", "値", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return value_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetValue(*typedValue); } }, 0.0f, maxValue_, 1.0f, true },
			{ "Position", "位置", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return position_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetPosition(*typedValue); } }, 0.0f, 0.0f, 1.0f },
			{ "Size", "サイズ", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return size_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetSize(*typedValue); } }, 0.0f, 4096.0f, 1.0f, true },
			{ "BackgroundColor", "背景色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return backgroundColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetBackgroundColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "FillColor", "塗り色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return fillColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetFillColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "BorderColor", "枠色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return borderColor_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetBorderColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "BorderThickness", "枠の太さ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return borderThickness_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetBorderThickness(*typedValue); } }, 0.0f, 64.0f, 0.1f, true },
			{ "FillDirection", "塗り方向", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return std::string(FillDirectionToString(fillDirection_)); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetFillDirection(FillDirectionFromString(*typedValue, fillDirection_)); } }, 0.0f, 0.0f, 0.1f, false, { { "LeftToRight", "左から右" }, { "RightToLeft", "右から左" }, { "TopToBottom", "上から下" }, { "BottomToTop", "下から上" } } },
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } },
		};
	}
}
