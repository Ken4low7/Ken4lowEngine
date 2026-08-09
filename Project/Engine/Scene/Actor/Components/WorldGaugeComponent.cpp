#define NOMINMAX
#include "WorldGaugeComponent.h"

#include "CameraManager.h"
#include "GameViewportConstants.h"
#include "Matrix4x4.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

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

		const char* FillDirectionToString(GaugeComponent::FillDirection direction)
		{
			switch (direction)
			{
			case GaugeComponent::FillDirection::RightToLeft:
				return "RightToLeft";
			case GaugeComponent::FillDirection::TopToBottom:
				return "TopToBottom";
			case GaugeComponent::FillDirection::BottomToTop:
				return "BottomToTop";
			case GaugeComponent::FillDirection::LeftToRight:
			default:
				return "LeftToRight";
			}
		}

		GaugeComponent::FillDirection FillDirectionFromString(const std::string& value, GaugeComponent::FillDirection defaultValue)
		{
			if (value == "LeftToRight") { return GaugeComponent::FillDirection::LeftToRight; }
			if (value == "RightToLeft") { return GaugeComponent::FillDirection::RightToLeft; }
			if (value == "TopToBottom") { return GaugeComponent::FillDirection::TopToBottom; }
			if (value == "BottomToTop") { return GaugeComponent::FillDirection::BottomToTop; }
			return defaultValue;
		}
	}

	WorldGaugeComponent::~WorldGaugeComponent() = default;

	void WorldGaugeComponent::Initialize()
	{
		SceneComponent::Initialize();
		gauge_.Initialize();
	}

	void WorldGaugeComponent::Draw()
	{
		// WorldGaugeComponentはActorWorldの2D描画パスでまとめて描画する
	}

	void WorldGaugeComponent::DrawScreenSpace()
	{
		if (!CanDrawScreenSpace())
		{
			return; // 非表示または無効な状態の場合は描画しない
		}

		Vector2 screenPosition{};
		if (!UpdateScreenPosition(screenPosition))
		{
			return; // カメラ背面など描画できない位置の場合は描画しない
		}

		gauge_.DrawGaugeAt(screenPosition);
	}

	bool WorldGaugeComponent::CanDrawScreenSpace() const
	{
		return IsActiveInHierarchy() && visible_;
	}

	void WorldGaugeComponent::DrawImGui()
	{
#ifdef USE_IMGUI
		SceneComponent::DrawImGui();

		ImGui::SeparatorText("ワールドゲージコンポーネント");
		ComponentPropertyUtility::DrawImGui(CreateProperties());
#endif // USE_IMGUI
	}

	void WorldGaugeComponent::Finalize()
	{
		gauge_.Finalize();
	}

	void WorldGaugeComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // WorldGaugeComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<WorldGaugeComponent*>(this)->CreateProperties(), outJson);
	}

	void WorldGaugeComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
	}

	bool WorldGaugeComponent::UpdateScreenPosition(Vector2& outScreenPosition) const
	{
		const Matrix4x4 viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		const Vector3 worldPosition = GetWorldPosition();

		const float clipX = worldPosition.x * viewProjection.m[0][0] + worldPosition.y * viewProjection.m[1][0] + worldPosition.z * viewProjection.m[2][0] + viewProjection.m[3][0];
		const float clipY = worldPosition.x * viewProjection.m[0][1] + worldPosition.y * viewProjection.m[1][1] + worldPosition.z * viewProjection.m[2][1] + viewProjection.m[3][1];
		const float clipW = worldPosition.x * viewProjection.m[0][3] + worldPosition.y * viewProjection.m[1][3] + worldPosition.z * viewProjection.m[2][3] + viewProjection.m[3][3];

		if (hideWhenBehindCamera_ && clipW <= 0.0f)
		{
			return false; // カメラ背面のGaugeは描画しない
		}

		if (std::fabs(clipW) <= 0.0001f)
		{
			return false; // NDC変換できない位置は描画しない
		}

		const float ndcX = clipX / clipW;
		const float ndcY = clipY / clipW;

		const float screenWidth = static_cast<float>(GameViewportConstants::Width);
		const float screenHeight = static_cast<float>(GameViewportConstants::Height);

		const Vector2 size = GetSize();
		outScreenPosition.x = (ndcX + 1.0f) * 0.5f * screenWidth + screenOffset_.x - size.x * 0.5f;
		outScreenPosition.y = (1.0f - ndcY) * 0.5f * screenHeight + screenOffset_.y - size.y * 0.5f;

		return true;
	}

	std::vector<ComponentProperty> WorldGaugeComponent::CreateProperties()
	{
		return {
			{ "MaxValue", "最大値", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return GetMaxValue(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetMaxValue(*typedValue); } }, 0.0001f, 1000000.0f, 1.0f, true },
			{ "Value", "値", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return GetValue(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetValue(*typedValue); } }, 0.0f, GetMaxValue(), 1.0f, true },
			{ "ScreenOffset", "スクリーンオフセット", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return screenOffset_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetScreenOffset(*typedValue); } }, 0.0f, 0.0f, 1.0f },
			{ "Size", "サイズ", ComponentPropertyType::Vector2, [this]() -> ComponentPropertyValue { return GetSize(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector2>(&value)) { SetSize(*typedValue); } }, 0.0f, 4096.0f, 1.0f, true },
			{ "BackgroundColor", "背景色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return GetBackgroundColor(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetBackgroundColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "FillColor", "塗り色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return GetFillColor(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetFillColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "BorderColor", "枠色", ComponentPropertyType::Vector4, [this]() -> ComponentPropertyValue { return GetBorderColor(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<Vector4>(&value)) { SetBorderColor(*typedValue); } }, 0.0f, 1.0f, 0.01f, true, {}, ComponentPropertyDisplay::Color },
			{ "BorderThickness", "枠の太さ", ComponentPropertyType::Float, [this]() -> ComponentPropertyValue { return GetBorderThickness(); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<float>(&value)) { SetBorderThickness(*typedValue); } }, 0.0f, 64.0f, 0.1f, true },
			{ "FillDirection", "塗り方向", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return std::string(FillDirectionToString(GetFillDirection())); }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetFillDirection(FillDirectionFromString(*typedValue, GetFillDirection())); } }, 0.0f, 0.0f, 0.1f, false, { { "LeftToRight", "左から右" }, { "RightToLeft", "右から左" }, { "TopToBottom", "上から下" }, { "BottomToTop", "下から上" } } },
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } },
			{ "HideWhenBehindCamera", "カメラ背面で非表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return hideWhenBehindCamera_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetHideWhenBehindCamera(*typedValue); } } },
		};
	}
}
