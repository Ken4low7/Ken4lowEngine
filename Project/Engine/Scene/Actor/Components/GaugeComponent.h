#pragma once
#include "ActorComponent.h"
#include "ComponentProperty.h"
#include "Sprite.h"
#include "Vector2.h"
#include "Vector4.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   Actorに画面固定のゲージ表示を追加するComponentクラス
	/// -------------------------------------------------------------
	class GaugeComponent : public ActorComponent
	{
	public:
		enum class FillDirection
		{
			LeftToRight,
			RightToLeft,
			TopToBottom,
			BottomToTop,
		};

	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~GaugeComponent() override;

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
			return "GaugeComponent"; // GaugeComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		float GetValue() const { return value_; }
		void SetValue(float value);

		float GetMaxValue() const { return maxValue_; }
		void SetMaxValue(float maxValue);

		const Vector2& GetPosition() const { return position_; }
		void SetPosition(const Vector2& position) { position_ = position; }

		const Vector2& GetSize() const { return size_; }
		void SetSize(const Vector2& size);

		const Vector4& GetBackgroundColor() const { return backgroundColor_; }
		void SetBackgroundColor(const Vector4& color) { backgroundColor_ = color; }

		const Vector4& GetFillColor() const { return fillColor_; }
		void SetFillColor(const Vector4& color) { fillColor_ = color; }

		const Vector4& GetBorderColor() const { return borderColor_; }
		void SetBorderColor(const Vector4& color) { borderColor_ = color; }

		float GetBorderThickness() const { return borderThickness_; }
		void SetBorderThickness(float thickness);

		FillDirection GetFillDirection() const { return fillDirection_; }
		void SetFillDirection(FillDirection direction) { fillDirection_ = direction; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

	public: /// ---------- ゲージ描画処理 ---------- ///

		void DrawGaugeAt(const Vector2& screenPosition);
		void NormalizeValues();

	private: /// ---------- 内部処理 ---------- ///

		void EnsureSprites();
		void DrawColoredRect(Sprite& sprite, const Vector2& position, const Vector2& size, const Vector4& color, const Vector2& anchor = { 0.0f, 0.0f });
		void DrawBorder(const Vector2& position, const Vector2& size);
		void CalculateFillRect(Vector2& outPosition, Vector2& outSize) const;
		std::vector<ComponentProperty> CreateProperties();

		static const char* FillDirectionToString(FillDirection direction);
		static FillDirection FillDirectionFromString(const std::string& value, FillDirection defaultValue);

	private: /// ---------- メンバ変数 ---------- ///

		float value_ = 100.0f;
		float maxValue_ = 100.0f;
		Vector2 position_ = { 100.0f, 100.0f };
		Vector2 size_ = { 240.0f, 24.0f };
		Vector4 backgroundColor_ = { 0.08f, 0.08f, 0.08f, 0.75f };
		Vector4 fillColor_ = { 0.1f, 0.85f, 0.35f, 1.0f };
		Vector4 borderColor_ = { 1.0f, 1.0f, 1.0f, 0.9f };
		float borderThickness_ = 2.0f;
		FillDirection fillDirection_ = FillDirection::LeftToRight;
		bool visible_ = true;

		std::unique_ptr<Sprite> backgroundSprite_;
		std::unique_ptr<Sprite> fillSprite_;
		std::array<std::unique_ptr<Sprite>, 4> borderSprites_;
	};
}
