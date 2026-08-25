#pragma once
#include "ModelComponent.h"
#include "Matrix4x4.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	struct WaterSurfaceSample
	{
		Vector3 worldPosition{};
		Vector3 worldNormal{ 0.0f, 1.0f, 0.0f };
		float signedDistance = 0.0f;
		float submersionDepth = 0.0f;
		bool isBelowSurface = false;
	};

	class WaterSurfaceComponent final : public ModelComponent
	{
	public:
		void Initialize() override
		{
			if (GetModelPath().empty() || GetModelPath() == "Sample/plane.gltf")
			{
				SetModelPath("Water/water_grid.obj");
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
			ApplyWaterMaterial(); // Waterは細分化Gridを通常Model描画へ載せ、専用Materialと頂点変形だけを追加する。
		}

		void Update(float deltaTime) override
		{
			waterTime_ += (std::max)(deltaTime, 0.0f);
			ModelComponent::Update(deltaTime);
			ApplyWaterMaterial();
		}

		void UpdateEditor(float deltaTime) override
		{
			waterTime_ += (std::max)(deltaTime, 0.0f);
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

		WaterSurfaceSample SampleSurfaceAtWorldPosition(const Vector3& worldPosition) const
		{
			WaterSurfaceSample sample{};
			const Matrix4x4 worldMatrix = Matrix4x4::MakeAffineMatrix(GetWorldScale(), GetWorldRotation(), GetWorldPosition());
			Matrix4x4 inverseWorld{};
			if (!Matrix4x4::TryInverse(worldMatrix, inverseWorld))
			{
				sample.worldPosition = GetWorldPosition();
				sample.signedDistance = worldPosition.y - sample.worldPosition.y;
				sample.submersionDepth = (std::max)(-sample.signedDistance, 0.0f);
				sample.isBelowSurface = sample.signedDistance < 0.0f;
				return sample;
			}

			const Vector3 localQuery = Vector3::Transform(worldPosition, inverseWorld);
			float baseX = localQuery.x;
			float baseY = localQuery.y;
			GerstnerEvaluation evaluation{};

			if (gerstnerEnabled_)
			{
				for (int iteration = 0; iteration < 3; ++iteration)
				{
					evaluation = EvaluateGerstner(baseX, baseY);
					baseX = localQuery.x - evaluation.offsetX;
					baseY = localQuery.y - evaluation.offsetY;
				}
				evaluation = EvaluateGerstner(baseX, baseY);
			}

			const Vector3 localSurfacePosition{
				baseX + evaluation.offsetX,
				baseY + evaluation.offsetY,
				evaluation.height
			};
			const Vector3 localSurfaceNormal = Vector3::NormalizeSafe(
				{ -evaluation.gradientX, -evaluation.gradientY, 1.0f },
				{ 0.0f, 0.0f, 1.0f });

			const Matrix4x4 normalMatrix = Matrix4x4::Transpose(inverseWorld);
			sample.worldPosition = Vector3::Transform(localSurfacePosition, worldMatrix);
			sample.worldNormal = Vector3::NormalizeSafe(TransformDirection(localSurfaceNormal, normalMatrix), { 0.0f, 1.0f, 0.0f });
			sample.signedDistance = Vector3::Dot(worldPosition - sample.worldPosition, sample.worldNormal);
			sample.submersionDepth = (std::max)(-sample.signedDistance, 0.0f);
			sample.isBelowSurface = sample.signedDistance < 0.0f;
			return sample; // CPU判定もGPUと同じGerstner位相を参照し、見た目の波面と入水判定を一致させる。
		}

		float GetSurfaceHeightAtWorldPosition(const Vector3& worldPosition) const
		{
			return SampleSurfaceAtWorldPosition(worldPosition).worldPosition.y;
		}

		void ToJson(nlohmann::json& outJson) const override
		{
			ModelComponent::ToJson(outJson);
			outJson["Class"] = GetClassTypeName();
			outJson["WaterColor"] = { waterColor_.x, waterColor_.y, waterColor_.z, waterColor_.w };
			outJson["Opacity"] = opacity_;
			outJson["Reflectivity"] = reflectivity_;
			outJson["Roughness"] = roughness_;
			outJson["WaveScale"] = waveScale_;
			outJson["WaveSpeed"] = waveSpeed_;
			outJson["NormalStrength"] = normalStrength_;
			outJson["FresnelF0"] = fresnelF0_;
			outJson["ReflectionDistortion"] = reflectionDistortion_;
			outJson["SecondaryWaveScale"] = secondaryWaveScale_;
			outJson["GerstnerEnabled"] = gerstnerEnabled_;
			outJson["GerstnerAmplitude"] = gerstnerAmplitude_;
			outJson["GerstnerWavelength"] = gerstnerWavelength_;
			outJson["GerstnerSpeed"] = gerstnerSpeed_;
			outJson["GerstnerSteepness"] = gerstnerSteepness_;
			outJson["GerstnerDirectionDegrees"] = gerstnerDirectionDegrees_;
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
			waveScale_ = inJson.value("WaveScale", waveScale_);
			waveSpeed_ = inJson.value("WaveSpeed", waveSpeed_);
			normalStrength_ = inJson.value("NormalStrength", normalStrength_);
			fresnelF0_ = inJson.value("FresnelF0", fresnelF0_);
			reflectionDistortion_ = inJson.value("ReflectionDistortion", reflectionDistortion_);
			secondaryWaveScale_ = inJson.value("SecondaryWaveScale", secondaryWaveScale_);
			gerstnerEnabled_ = inJson.value("GerstnerEnabled", gerstnerEnabled_);
			gerstnerAmplitude_ = inJson.value("GerstnerAmplitude", gerstnerAmplitude_);
			gerstnerWavelength_ = inJson.value("GerstnerWavelength", gerstnerWavelength_);
			gerstnerSpeed_ = inJson.value("GerstnerSpeed", gerstnerSpeed_);
			gerstnerSteepness_ = inJson.value("GerstnerSteepness", gerstnerSteepness_);
			gerstnerDirectionDegrees_ = inJson.value("GerstnerDirectionDegrees", gerstnerDirectionDegrees_);
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
			ImGui::SeparatorText("Water Detail Waves");
			ImGui::DragFloat("波の密度##WaterSurface", &waveScale_, 0.01f, 0.01f, 4.0f);
			ImGui::DragFloat("波の速度##WaterSurface", &waveSpeed_, 0.01f, 0.0f, 8.0f);
			ImGui::DragFloat("法線の強さ##WaterSurface", &normalStrength_, 0.005f, 0.0f, 1.0f);
			ImGui::DragFloat("副波スケール##WaterSurface", &secondaryWaveScale_, 0.01f, 0.05f, 4.0f);
			ImGui::DragFloat("Fresnel F0##WaterSurface", &fresnelF0_, 0.001f, 0.0f, 0.15f);
			ImGui::DragFloat("反射ゆらぎ##WaterSurface", &reflectionDistortion_, 0.005f, 0.0f, 1.0f);
			ImGui::SeparatorText("Gerstner Wave");
			ImGui::Checkbox("立体波を有効化##WaterSurface", &gerstnerEnabled_);
			ImGui::DragFloat("波高##WaterSurface", &gerstnerAmplitude_, 0.002f, 0.0f, 0.25f);
			ImGui::DragFloat("波長##WaterSurface", &gerstnerWavelength_, 0.01f, 0.1f, 4.0f);
			ImGui::DragFloat("進行速度##WaterSurface", &gerstnerSpeed_, 0.01f, 0.0f, 8.0f);
			ImGui::DragFloat("尖り具合##WaterSurface", &gerstnerSteepness_, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("進行方向(度)##WaterSurface", &gerstnerDirectionDegrees_, 1.0f, -180.0f, 180.0f);
			ImGui::TextDisabled("Gerstner WaveはGrid頂点そのものを変形し、法線波は表面の細かい揺らぎを担当します。");
			ImGui::TextDisabled("同じActorへPlanarReflectionComponentを追加すると水面へ局所反射を適用します。");
#endif
			ApplyWaterMaterial();
		}

	private:
		struct GerstnerEvaluation
		{
			float offsetX = 0.0f;
			float offsetY = 0.0f;
			float height = 0.0f;
			float gradientX = 0.0f;
			float gradientY = 0.0f;
		};

		static Vector3 TransformDirection(const Vector3& direction, const Matrix4x4& matrix)
		{
			return {
				direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0],
				direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1],
				direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2]
			};
		}

		static void AccumulateGerstner(
			float baseX,
			float baseY,
			float directionX,
			float directionY,
			float amplitude,
			float wavelength,
			float speed,
			float steepness,
			float time,
			float phaseOffset,
			GerstnerEvaluation& evaluation)
		{
			constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
			const float safeWavelength = (std::max)(wavelength, 0.001f);
			const float directionLength = std::sqrt(directionX * directionX + directionY * directionY);
			if (directionLength <= 0.0001f) return;

			directionX /= directionLength;
			directionY /= directionLength;
			const float waveNumber = twoPi / safeWavelength;
			const float phase = waveNumber * (directionX * baseX + directionY * baseY) - time * speed + phaseOffset;
			const float sinePhase = std::sin(phase);
			const float cosinePhase = std::cos(phase);
			const float clampedSteepness = std::clamp(steepness, 0.0f, 1.0f);

			evaluation.offsetX += directionX * (clampedSteepness * amplitude * cosinePhase);
			evaluation.offsetY += directionY * (clampedSteepness * amplitude * cosinePhase);
			evaluation.height += amplitude * sinePhase;
			evaluation.gradientX += directionX * (amplitude * waveNumber * cosinePhase);
			evaluation.gradientY += directionY * (amplitude * waveNumber * cosinePhase);
		}

		GerstnerEvaluation EvaluateGerstner(float baseX, float baseY) const
		{
			GerstnerEvaluation evaluation{};
			if (!gerstnerEnabled_) return evaluation;

			const float amplitude = std::clamp(gerstnerAmplitude_, 0.0f, 0.25f);
			const float wavelength = (std::max)(gerstnerWavelength_, 0.1f);
			const float speed = (std::max)(gerstnerSpeed_, 0.0f);
			const float steepness = std::clamp(gerstnerSteepness_, 0.0f, 1.0f);
			const float directionRadians = gerstnerDirectionDegrees_ * std::numbers::pi_v<float> / 180.0f;
			const float primaryX = std::cos(directionRadians);
			const float primaryY = std::sin(directionRadians);

			AccumulateGerstner(baseX, baseY, primaryX, primaryY, amplitude, wavelength, speed, steepness, waterTime_, 0.0f, evaluation);

			float secondaryX = -primaryY + primaryX * 0.35f;
			float secondaryY = primaryX + primaryY * 0.35f;
			const float secondaryLength = std::sqrt(secondaryX * secondaryX + secondaryY * secondaryY);
			if (secondaryLength > 0.0001f)
			{
				secondaryX /= secondaryLength;
				secondaryY /= secondaryLength;
			}
			AccumulateGerstner(
				baseX,
				baseY,
				secondaryX,
				secondaryY,
				amplitude * 0.45f,
				wavelength * 0.58f,
				speed * 1.35f,
				steepness * 0.7f,
				waterTime_,
				1.7f,
				evaluation);
			return evaluation;
		}

		void ApplyWaterMaterial()
		{
			Object3D* object3D = GetObject3D();
			if (!object3D) return;

			const float opacity = std::clamp(opacity_, 0.05f, 1.0f);
			const float directionRadians = gerstnerDirectionDegrees_ * std::numbers::pi_v<float> / 180.0f;
			object3D->SetColor({ waterColor_.x, waterColor_.y, waterColor_.z, opacity });
			object3D->SetPbrEnabled(false);
			object3D->SetReflectivity(std::clamp(reflectivity_, 0.0f, 1.0f));
			object3D->SetRoughness(std::clamp(roughness_, 0.0f, 1.0f));
			object3D->SetCullMode(MaterialCullMode::None);
			object3D->SetAlphaBlendEnabled(true);
			object3D->SetWaterSurfaceState(
				true,
				waterTime_,
				(std::max)(waveScale_, 0.01f),
				(std::max)(waveSpeed_, 0.0f),
				std::clamp(normalStrength_, 0.0f, 1.0f),
				std::clamp(fresnelF0_, 0.0f, 0.15f),
				std::clamp(reflectionDistortion_, 0.0f, 1.0f),
				(std::max)(secondaryWaveScale_, 0.05f),
				gerstnerEnabled_,
				std::clamp(gerstnerAmplitude_, 0.0f, 0.25f),
				(std::max)(gerstnerWavelength_, 0.1f),
				(std::max)(gerstnerSpeed_, 0.0f),
				std::clamp(gerstnerSteepness_, 0.0f, 1.0f),
				std::cos(directionRadians),
				std::sin(directionRadians));
		}

		Vector4 waterColor_{ 0.035f, 0.24f, 0.34f, 1.0f };
		float opacity_ = 0.68f;
		float reflectivity_ = 0.45f;
		float roughness_ = 0.08f;
		float waveScale_ = 0.35f;
		float waveSpeed_ = 1.0f;
		float normalStrength_ = 0.12f;
		float fresnelF0_ = 0.02f;
		float reflectionDistortion_ = 0.08f;
		float secondaryWaveScale_ = 0.67f;
		float gerstnerAmplitude_ = 0.035f;
		float gerstnerWavelength_ = 0.8f;
		float gerstnerSpeed_ = 1.25f;
		float gerstnerSteepness_ = 0.35f;
		float gerstnerDirectionDegrees_ = 28.0f;
		float waterTime_ = 0.0f;
		bool gerstnerEnabled_ = true;
		bool loadedFromJson_ = false;
	};
} // namespace Ken4lowEngine
