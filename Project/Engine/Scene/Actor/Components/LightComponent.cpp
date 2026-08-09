#include "LightComponent.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;

		std::string LightTypeToString(LightComponent::LightType lightType)
		{
			switch (lightType)
			{
			case LightComponent::LightType::None: return "None";
			case LightComponent::LightType::Directional: return "Directional";
			case LightComponent::LightType::Point: return "Point";
			case LightComponent::LightType::Spot: return "Spot";
			case LightComponent::LightType::RectArea: return "RectArea";
			case LightComponent::LightType::SphereArea: return "SphereArea";
			default: return "Point";
			}
		}

		LightComponent::LightType LightTypeFromString(const std::string& lightType)
		{
			if (lightType == "None") { return LightComponent::LightType::None; }
			if (lightType == "Directional") { return LightComponent::LightType::Directional; }
			if (lightType == "Point") { return LightComponent::LightType::Point; }
			if (lightType == "Spot") { return LightComponent::LightType::Spot; }
			if (lightType == "RectArea") { return LightComponent::LightType::RectArea; }
			if (lightType == "SphereArea") { return LightComponent::LightType::SphereArea; }
			return LightComponent::LightType::Point;
		}

		bool UsesRange(LightComponent::LightType lightType)
		{
			return lightType == LightComponent::LightType::Point ||
				lightType == LightComponent::LightType::Spot ||
				lightType == LightComponent::LightType::RectArea ||
				lightType == LightComponent::LightType::SphereArea;
		}

		bool UsesDirection(LightComponent::LightType lightType)
		{
			return lightType == LightComponent::LightType::Directional ||
				lightType == LightComponent::LightType::Spot ||
				lightType == LightComponent::LightType::RectArea;
		}

		bool UsesAreaSize(LightComponent::LightType lightType)
		{
			return lightType == LightComponent::LightType::RectArea ||
				lightType == LightComponent::LightType::SphereArea;
		}

		std::vector<ComponentPropertyChoice> LightTypeChoices()
		{
			return {
				{ "None", "なし / None" },
				{ "Directional", "平行光源 / Directional" },
				{ "Point", "点光源 / Point" },
				{ "Spot", "スポットライト / Spot" },
				{ "RectArea", "矩形エリアライト / RectArea" },
				{ "SphereArea", "球エリアライト / SphereArea" },
			};
		}
	}

	void LightComponent::DrawImGui()
	{
		SceneComponent::DrawImGui(); // ライトの基準位置はSceneComponentで編集し、LightComponentは光源パラメータだけを担当する。

#ifdef USE_IMGUI
		ImGui::SeparatorText("ライトコンポーネント");
		ImGui::TextUnformatted("Individual light values are edited here and synced to LightManager before rendering.");
		ImGui::TextDisabled("LightManager shows Component Lights as read-only debug data.");

		bool changed = false;

		std::string currentTypeName = LightTypeToString(lightType_);
		std::string previewName = currentTypeName;
		for (const ComponentPropertyChoice& choice : LightTypeChoices())
		{
			if (choice.value == currentTypeName)
			{
				previewName = choice.displayName.empty() ? choice.value : choice.displayName;
				break;
			}
		}

		if (ImGui::BeginCombo("ライト種類##LightComponentType", previewName.c_str()))
		{
			for (const ComponentPropertyChoice& choice : LightTypeChoices())
			{
				const bool selected = choice.value == currentTypeName;
				const std::string label = choice.displayName.empty() ? choice.value : choice.displayName;
				if (ImGui::Selectable(label.c_str(), selected))
				{
					lightType_ = LightTypeFromString(choice.value); // Actor Details側を個別ライト編集の主役にするため、種類変更もComponentへ直接反映する。
					Sanitize();
					changed = true;
				}
				if (selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		bool editEnabled = enabled_;
		if (ImGui::Checkbox("有効##LightComponentEnabled", &editEnabled))
		{
			enabled_ = editEnabled; // 有効状態は次のSyncLightComponentsToLightManagerでGPU転送対象へ反映される。
			changed = true;
		}

		float editColor[3] = { color_.x, color_.y, color_.z };
		if (ImGui::DragFloat3("色##LightComponentColor", editColor, 0.01f, 0.0f, 1.0f))
		{
			color_ = { editColor[0], editColor[1], editColor[2] }; // 色は既存JSONキーを変えず、Component内部値だけ更新する。
			changed = true;
		}

		if (ImGui::DragFloat("強さ##LightComponentIntensity", &intensity_, 0.05f, 0.0f, 100.0f))
		{
			intensity_ = std::max(intensity_, 0.0f); // 負の強度でライトが反転しないよう、既存の安全範囲へ丸める。
			changed = true;
		}

		if (UsesRange(lightType_))
		{
			if (ImGui::DragFloat("範囲##LightComponentRange", &range_, 0.1f, 0.0f, 1000.0f))
			{
				Sanitize();
				changed = true;
			}
			if (ImGui::DragFloat("減衰##LightComponentDecay", &decay_, 0.05f, 0.0f, 10.0f))
			{
				Sanitize();
				changed = true;
			}
		}
		else
		{
			ImGui::TextDisabled("Range / Decay are used by Point, Spot, and Area lights.");
		}

		if (lightType_ == LightType::Spot)
		{
			if (ImGui::DragFloat("内角度##LightComponentInnerAngle", &innerAngle_, 0.1f, 0.0f, 179.0f))
			{
				Sanitize();
				changed = true;
			}
			if (ImGui::DragFloat("外角度##LightComponentOuterAngle", &outerAngle_, 0.1f, 0.1f, 179.0f))
			{
				Sanitize();
				changed = true;
			}
		}
		else
		{
			ImGui::TextDisabled("Spot angles are shown only for Spot lights.");
		}

		if (UsesAreaSize(lightType_))
		{
			float editAreaSize[3] = { areaSize_.x, areaSize_.y, areaSize_.z };
			if (ImGui::DragFloat3("エリアサイズ##LightComponentAreaSize", editAreaSize, 0.05f, 0.0f, 50.0f))
			{
				areaSize_ = { editAreaSize[0], editAreaSize[1], editAreaSize[2] };
				Sanitize();
				changed = true;
			}
		}
		else
		{
			ImGui::TextDisabled("Area Size is shown only for Rect/Sphere area lights.");
		}

		if (UsesDirection(lightType_))
		{
			const Vector3 direction = CalculateDirection();
			ImGui::Text("ライト方向: %.2f, %.2f, %.2f", direction.x, direction.y, direction.z);
		}

		if (changed)
		{
			// ActorWorld::SyncLightComponentsToLightManager がこの値を収集し、LightManagerではread-only debugとして表示する。
			ImGui::TextColored(ImVec4(0.75f, 1.0f, 0.75f, 1.0f), "LightComponent values updated. They will be synced before the next draw.");
		}
#endif // USE_IMGUI
	}

	void LightComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson); // SceneComponent共通情報をJSONへ保存する

		outJson["Class"] = GetClassTypeName(); // LightComponentとして保存する
		ComponentPropertyUtility::ToJson(const_cast<LightComponent*>(this)->CreateProperties(true), outJson); // 種類を切り替えてもRange/Spot/Area設定を失わないよう全ライト項目を保存する
	}

	void LightComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson); // SceneComponent共通情報をJSONから復元する

		if (inJson.contains("LightType") && inJson["LightType"].is_string())
		{
			lightType_ = LightTypeFromString(inJson["LightType"].get<std::string>()); // 不正な種類はPointへフォールバックする
		}

		ComponentPropertyUtility::FromJson(CreateProperties(true), inJson);
		Sanitize();
	}

	std::vector<ComponentProperty> LightComponent::CreateProperties(bool includeAll)
	{
		std::vector<ComponentProperty> properties = {
			ComponentProperty{
				"LightType",
				"ライト種類",
				ComponentPropertyType::String,
				[this]() -> ComponentPropertyValue { return LightTypeToString(lightType_); },
				[this](const ComponentPropertyValue& value)
				{
					if (const std::string* typedValue = std::get_if<std::string>(&value))
					{
						lightType_ = LightTypeFromString(*typedValue); // LightManagerの既存lightTypeに対応する種類へ変換する
					}
				},
				0.0f,
				0.0f,
				0.1f,
				false,
				LightTypeChoices()
			},
			ComponentProperty{
				"Enabled",
				"有効",
				ComponentPropertyType::Bool,
				[this]() -> ComponentPropertyValue { return enabled_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const bool* typedValue = std::get_if<bool>(&value))
					{
						enabled_ = *typedValue; // ライトの有効状態を更新する
					}
				}
			},
			ComponentProperty{
				"Color",
				"色",
				ComponentPropertyType::Vector3,
				[this]() -> ComponentPropertyValue { return color_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const Vector3* typedValue = std::get_if<Vector3>(&value))
					{
						color_ = *typedValue; // ライト色を更新する
					}
				},
				0.0f,
				1.0f,
				0.01f,
				true
			},
			ComponentProperty{
				"Intensity",
				"強さ",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return intensity_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						intensity_ = *typedValue; // ライトの強さを更新する
					}
				},
				0.0f,
				100.0f,
				0.05f,
				true
			},
		};

		if (includeAll || UsesRange(lightType_))
		{
			properties.push_back(ComponentProperty{
				"Range",
				"範囲",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return range_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						range_ = *typedValue; // ライトの届く範囲を更新する
						Sanitize();
					}
				},
				0.0f,
				1000.0f,
				0.1f,
				true
			});

			properties.push_back(ComponentProperty{
				"Decay",
				"減衰",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return decay_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						decay_ = *typedValue; // 距離減衰の強さを更新する
						Sanitize();
					}
				},
				0.0f,
				10.0f,
				0.05f,
				true
			});
		}

		if (includeAll || lightType_ == LightType::Spot)
		{
			properties.push_back(ComponentProperty{
				"InnerAngle",
				"内角度",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return innerAngle_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						innerAngle_ = *typedValue; // SpotLightの内側角度を更新する
						Sanitize();
					}
				},
				0.0f,
				179.0f,
				0.1f,
				true
			});

			properties.push_back(ComponentProperty{
				"OuterAngle",
				"外角度",
				ComponentPropertyType::Float,
				[this]() -> ComponentPropertyValue { return outerAngle_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const float* typedValue = std::get_if<float>(&value))
					{
						outerAngle_ = *typedValue; // SpotLightの外側角度を更新する
						Sanitize();
					}
				},
				0.1f,
				179.0f,
				0.1f,
				true
			});
		}

		if (includeAll || UsesAreaSize(lightType_))
		{
			properties.push_back(ComponentProperty{
				"AreaSize",
				"エリアサイズ",
				ComponentPropertyType::Vector3,
				[this]() -> ComponentPropertyValue { return areaSize_; },
				[this](const ComponentPropertyValue& value)
				{
					if (const Vector3* typedValue = std::get_if<Vector3>(&value))
					{
						areaSize_ = *typedValue; // 既存AreaLight近似のサイズを更新する
						Sanitize();
					}
				},
				0.0f,
				50.0f,
				0.05f,
				true
			});
		}

		return properties;
	}

	void LightComponent::Sanitize()
	{
		range_ = std::max(range_, 0.0f);
		decay_ = std::clamp(decay_, 0.0f, 10.0f);
		outerAngle_ = std::clamp(outerAngle_, 0.1f, 179.0f);
		innerAngle_ = std::clamp(innerAngle_, 0.0f, outerAngle_);
		areaSize_.x = std::max(areaSize_.x, 0.0f);
		areaSize_.y = std::max(areaSize_.y, 0.0f);
		areaSize_.z = std::max(areaSize_.z, 0.0f);
	}

	Vector3 LightComponent::CalculateDirection() const
	{
		const Vector3& rotation = GetWorldRotation();
		const float pitch = rotation.x;
		const float yaw = rotation.y;
		const float cp = std::cos(pitch);
		const float sp = std::sin(pitch);
		const float cy = std::cos(yaw);
		const float sy = std::sin(yaw);

		return Vector3::NormalizeSafe({ sy * cp, -sp, cy * cp }, { 0.0f, -1.0f, 0.0f });
	}
}
