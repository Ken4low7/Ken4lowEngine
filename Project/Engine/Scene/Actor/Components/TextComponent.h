#pragma once
#include "ActorComponent.h"
#include "ComponentProperty.h"
#include "TextSpriteDrawer.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   Actorに画面固定の文字表示を追加するComponentクラス
	/// -------------------------------------------------------------
	class TextComponent : public ActorComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~TextComponent() override;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// TextComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		/// <summary>
		/// TextComponentの通常描画処理。
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
		/// TextComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// TextComponentの終了処理。
		/// </summary>
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		std::string GetClassTypeName() const override
		{
			return "TextComponent"; // TextComponentとして保存する。
		}

		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetText() const { return text_; }
		void SetText(const std::string& text) { text_ = text; }

		const Vector2& GetPosition() const { return position_; }
		void SetPosition(const Vector2& position) { position_ = position; }

		float GetFontSize() const { return fontSize_; }
		void SetFontSize(float fontSize);

		const Vector4& GetColor() const { return color_; }
		void SetColor(const Vector4& color) { color_ = color; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

		const Vector2& GetAnchor() const { return anchor_; }
		void SetAnchor(const Vector2& anchor);

		const std::string& GetFontName() const { return fontName_; }
		void SetFontName(const std::string& fontName);

	private: /// ---------- 内部処理 ---------- ///

		void EnsureTextDrawer();
		Vector2 ApplyAnchor(const Vector2& position);
		std::vector<ComponentProperty> CreateProperties();

	private: /// ---------- メンバ変数 ---------- ///

		std::string text_ = "Text";
		Vector2 position_ = { 100.0f, 100.0f };
		float fontSize_ = 32.0f;
		Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool visible_ = true;
		Vector2 anchor_ = { 0.0f, 0.0f };
		std::string fontName_ = "DotGothic16";

		std::unique_ptr<TextSpriteDrawer> textDrawer_;
		std::string loadedFontName_;
		bool textDrawerReady_ = false;
	};
}
