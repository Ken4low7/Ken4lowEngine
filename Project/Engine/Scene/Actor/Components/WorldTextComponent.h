#pragma once
#include "ComponentProperty.h"
#include "SceneComponent.h"
#include "TextSpriteDrawer.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   3D位置に追従して画面上へ文字を表示するComponentクラス
	/// -------------------------------------------------------------
	class WorldTextComponent : public SceneComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~WorldTextComponent() override;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// WorldTextComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// WorldTextComponentの通常描画処理。
		/// </summary>
		void Draw() override;

		/// <summary>
		/// Screen Space Text用の描画処理。
		/// </summary>
		void DrawScreenSpace();

		/// <summary>
		/// Screen Space Textとして描画できる状態か確認する。
		/// </summary>
		bool CanDrawScreenSpace() const;

		/// <summary>
		/// WorldTextComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// WorldTextComponentの終了処理。
		/// </summary>
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "WorldTextComponent"; // WorldTextComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetText() const { return text_; }
		void SetText(const std::string& text) { text_ = text; }

		const Vector2& GetScreenOffset() const { return screenOffset_; }
		void SetScreenOffset(const Vector2& screenOffset) { screenOffset_ = screenOffset; }

		float GetFontSize() const { return fontSize_; }
		void SetFontSize(float fontSize);

		const Vector4& GetColor() const { return color_; }
		void SetColor(const Vector4& color) { color_ = color; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

		bool IsHideWhenBehindCamera() const { return hideWhenBehindCamera_; }
		void SetHideWhenBehindCamera(bool hideWhenBehindCamera) { hideWhenBehindCamera_ = hideWhenBehindCamera; }

		const Vector2& GetAnchor() const { return anchor_; }
		void SetAnchor(const Vector2& anchor);

		const std::string& GetFontName() const { return fontName_; }
		void SetFontName(const std::string& fontName);

	private: /// ---------- 内部処理 ---------- ///

		bool UpdateScreenPosition(Vector2& outScreenPosition) const;
		void EnsureTextDrawer();
		Vector2 ApplyAnchor(const Vector2& position);
		std::vector<ComponentProperty> CreateProperties();

	private: /// ---------- メンバ変数 ---------- ///

		std::string text_ = "World Text";
		Vector2 screenOffset_ = { 0.0f, -80.0f };
		float fontSize_ = 32.0f;
		Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool visible_ = true;
		bool hideWhenBehindCamera_ = true;
		Vector2 anchor_ = { 0.5f, 0.5f };
		std::string fontName_ = "DotGothic16";

		std::unique_ptr<TextSpriteDrawer> textDrawer_;
		std::string loadedFontName_;
		bool textDrawerReady_ = false;
	};
}
