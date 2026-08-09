#pragma once
#include "SceneComponent.h"
#include "ComponentProperty.h"

#include "Vector3.h"
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///			Actorにライト情報を追加するComponentクラス
	///			個別ライトの編集元として、Actor Details上でLightType/Color/Intensity/Range等を管理する。
	/// -------------------------------------------------------------
	class LightComponent : public SceneComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// LightComponentのImGui描画処理
		/// </summary>
		void DrawImGui() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "LightComponent"; // LightComponentとして保存する
		}

		/// <summary>
		/// LightComponent固有情報をJSONへ保存する
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからLightComponent固有情報を復元する
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		enum class LightType : uint32_t
		{
			None = 0,
			Directional = 1,
			Point = 2,
			Spot = 3,
			RectArea = 4,
			SphereArea = 5,
		};

		const Vector3& GetColor() const { return color_; }
		void SetColor(const Vector3& color) { color_ = color; }

		float GetIntensity() const { return intensity_; }
		void SetIntensity(float intensity) { intensity_ = intensity; }

		float GetRange() const { return range_; }
		void SetRange(float range) { range_ = range; }

		bool IsEnabled() const { return enabled_; }
		void SetEnabled(bool enabled) { enabled_ = enabled; }

		LightType GetLightType() const { return lightType_; }
		uint32_t GetLightTypeValue() const { return static_cast<uint32_t>(lightType_); }
		void SetLightType(LightType lightType) { lightType_ = lightType; }

		float GetDecay() const { return decay_; }
		float GetInnerAngle() const { return innerAngle_; }
		float GetOuterAngle() const { return outerAngle_; }
		const Vector3& GetAreaSize() const { return areaSize_; }
		Vector3 CalculateDirection() const;

	private: /// ---------- メンバ変数 ---------- ///

		std::vector<ComponentProperty> CreateProperties(bool includeAll = false);
		void Sanitize();

		LightType lightType_ = LightType::Point;
		Vector3 color_ = { 1.0f, 1.0f, 1.0f };
		float intensity_ = 1.0f;
		float range_ = 10.0f;
		float decay_ = 1.0f;
		float innerAngle_ = 15.0f;
		float outerAngle_ = 30.0f;
		Vector3 areaSize_ = { 2.0f, 2.0f, 1.0f };
		bool enabled_ = true;
	};
}
