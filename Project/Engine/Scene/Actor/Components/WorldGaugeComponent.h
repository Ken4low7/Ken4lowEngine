#pragma once
#include "ComponentProperty.h"
#include "GaugeComponent.h"
#include "SceneComponent.h"
#include "Vector2.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   3D位置に追従して画面上へゲージを表示するComponentクラス
	/// -------------------------------------------------------------
	class WorldGaugeComponent : public SceneComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~WorldGaugeComponent() override;

	public: /// ---------- メンバ関数 ---------- ///

		void Initialize() override;
		void Draw() override;
		void DrawScreenSpace();
		bool CanDrawScreenSpace() const;
		void DrawImGui() override;
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "WorldGaugeComponent"; // WorldGaugeComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		float GetValue() const { return gauge_.GetValue(); }
		void SetValue(float value) { gauge_.SetValue(value); }

		float GetMaxValue() const { return gauge_.GetMaxValue(); }
		void SetMaxValue(float maxValue) { gauge_.SetMaxValue(maxValue); }

		const Vector2& GetScreenOffset() const { return screenOffset_; }
		void SetScreenOffset(const Vector2& screenOffset) { screenOffset_ = screenOffset; }

		const Vector2& GetSize() const { return gauge_.GetSize(); }
		void SetSize(const Vector2& size) { gauge_.SetSize(size); }

		const Vector4& GetBackgroundColor() const { return gauge_.GetBackgroundColor(); }
		void SetBackgroundColor(const Vector4& color) { gauge_.SetBackgroundColor(color); }

		const Vector4& GetFillColor() const { return gauge_.GetFillColor(); }
		void SetFillColor(const Vector4& color) { gauge_.SetFillColor(color); }

		const Vector4& GetBorderColor() const { return gauge_.GetBorderColor(); }
		void SetBorderColor(const Vector4& color) { gauge_.SetBorderColor(color); }

		float GetBorderThickness() const { return gauge_.GetBorderThickness(); }
		void SetBorderThickness(float thickness) { gauge_.SetBorderThickness(thickness); }

		GaugeComponent::FillDirection GetFillDirection() const { return gauge_.GetFillDirection(); }
		void SetFillDirection(GaugeComponent::FillDirection direction) { gauge_.SetFillDirection(direction); }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

		bool IsHideWhenBehindCamera() const { return hideWhenBehindCamera_; }
		void SetHideWhenBehindCamera(bool hideWhenBehindCamera) { hideWhenBehindCamera_ = hideWhenBehindCamera; }

	private: /// ---------- 内部処理 ---------- ///

		bool UpdateScreenPosition(Vector2& outScreenPosition) const;
		std::vector<ComponentProperty> CreateProperties();

	private: /// ---------- メンバ変数 ---------- ///

		GaugeComponent gauge_;
		Vector2 screenOffset_ = { 0.0f, -72.0f };
		bool visible_ = true;
		bool hideWhenBehindCamera_ = true;
	};
}
