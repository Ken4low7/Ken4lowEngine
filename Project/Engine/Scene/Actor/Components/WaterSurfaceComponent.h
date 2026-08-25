#pragma once
#include "ModelComponent.h"

#include <algorithm>
#include <numbers>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	class WaterSurfaceComponent final : public ModelComponent
	{
	public:
		void Initialize() override
		{
			if (GetModelPath().empty())
			{
				SetModelPath("Sample/plane.gltf");
			}

			if (!loadedFromJson_)
			{
				if (GetLocalRotation() == Vector3{ 0.0f, 0.0f, 0.0f })
				{
					SetLocalRotation({ -std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f });
				}
				if (GetLocalScale() == Vector3{ 1.0f, 1.0f, 1.0f })
				{
					SetLocalScale({ 10.0f, 10.0f, 10.0f });
				}
			}

			ModelComponent::Initialize();
			ApplyWaterMaterial(); // Waterは通常Model描画を再利用し、Material分類だけ透明水面へ固定する。
		}

		void Update(float deltaTime) override
		{
			ModelComponent::Update(deltaTime);
			ApplyWaterMaterial();
		}

		void UpdateEditor(float deltaTime) override
		{
			ModelComponent::UpdateEditor(deltaTime);
			ApplyWaterMaterial();
		}

		void PostPhysicsUpdate(float deltaTime) override
		{
			ModelComponent::PostPhysicsUpdate(deltaTime);
			ApplyWaterMaterial();
		}

		void DrawShadow() override {}
		bool SupportsShadowCasting() const override { return false; }

		std::string GetClassTypeName() const override { return "WaterSurfaceComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			ModelComponent::ToJson(outJson);
			outJson["Class"] = GetClassTypeName();
			outJson["WaterColor"] = { waterColor_.x, waterColor_.y, waterColor_.z, waterColor_.w };
			outJson["Opacity"] = opacity_;
			outJson["Reflectivity"] = reflectivity_;
			outJson["Roughness"] = roughness_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			loadedFromJson_ = true;
			ModelComponent::FromJson(inJson);

			const auto colorIt = inJson.find("WaterColor");
			if (colorIt != inJson.end() && colorIt->is_array() && colorIt->size() >= 4)
			{
				waterColor_ = {
					(*colorIt)[0].get<float>(),
					(*colorIt)[1].get<float>(),
					(*colorIt)[2].get<float>(),
					(*colorIt)[3].get<float>(),
				};
			}
			opacity_ = inJson.value("Opacity", opacity_);
			reflectivity_ = inJson.value("Reflectivity", reflectivity_);
			roughness_ = inJson.value("Roughness", roughness_);
			ApplyWaterMaterial();
		}

		void DrawImGui() override
		{
			ModelComponent::DrawImGui();
#ifdef USE_IMGUI
			ImGui::SeparatorText("Water Surface");
			ImGui::ColorEdit4("水面色##WaterSurface", &waterColor_.x);
			ImGui::DragFloat("透明度##WaterSurface", &opacity_, 0.01f, 0.05f, 1.0f);
			ImGui::DragFloat("反射率##WaterSurface", &reflectivity_, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("粗さ##WaterSurface", &roughness_, 0.01f, 0.0f, 1.0f);
			ImGui::TextDisabled("同じActorへPlanarReflectionComponentを追加すると水面へ局所反射を適用します。");
			ImGui::TextDisabled("次段階で波法線・屈折・深度吸収をこのComponentへ追加します。");
#endif
			ApplyWaterMaterial();
		}

	private:
		void ApplyWaterMaterial()
		{
			Object3D* object3D = GetObject3D();
			if (!object3D) return;

			const float opacity = std::clamp(opacity_, 0.05f, 1.0f);
			object3D->SetColor({ waterColor_.x, waterColor_.y, waterColor_.z, opacity });
			object3D->SetPbrEnabled(false);
			object3D->SetReflectivity(std::clamp(reflectivity_, 0.0f, 1.0f));
			object3D->SetRoughness(std::clamp(roughness_, 0.0f, 1.0f));
			object3D->SetCullMode(MaterialCullMode::None);
			object3D->SetAlphaBlendEnabled(true);
		}

		Vector4 waterColor_{ 0.035f, 0.24f, 0.34f, 1.0f };
		float opacity_ = 0.68f;
		float reflectivity_ = 0.45f;
		float roughness_ = 0.08f;
		bool loadedFromJson_ = false;
	};
} // namespace Ken4lowEngine
