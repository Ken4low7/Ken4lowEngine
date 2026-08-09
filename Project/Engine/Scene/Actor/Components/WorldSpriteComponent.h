#pragma once
#include "ComponentProperty.h"
#include "SceneComponent.h"
#include "Sprite.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   3D位置に追従して画面上へSpriteを表示するComponentクラス
	/// -------------------------------------------------------------
	class WorldSpriteComponent : public SceneComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~WorldSpriteComponent() override;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// WorldSpriteComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		void Update(float deltaTime) override;

		/// <summary>
		/// WorldSpriteComponentの通常描画処理。
		/// </summary>
		void Draw() override;

		/// <summary>
		/// Screen Space Sprite用の描画処理。
		/// </summary>
		void DrawScreenSpace();

		/// <summary>
		/// Screen Space Spriteとして描画できる状態か確認する。
		/// </summary>
		bool CanDrawScreenSpace() const;

		/// <summary>
		/// WorldSpriteComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// WorldSpriteComponentの終了処理。
		/// </summary>
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する。
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "WorldSpriteComponent"; // WorldSpriteComponentとして保存する。
		}

		/// <summary>
		/// WorldSpriteComponent固有情報をJSONへ保存する。
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからWorldSpriteComponent固有情報を復元する。
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetTexturePath() const { return texturePath_; }
		void SetTexturePath(const std::string& texturePath);

		const Vector2& GetScreenOffset() const { return screenOffset_; }
		void SetScreenOffset(const Vector2& screenOffset) { screenOffset_ = screenOffset; }

		const Vector2& GetSize() const { return size_; }
		void SetSize(const Vector2& size);

		const Vector4& GetColor() const { return color_; }
		void SetColor(const Vector4& color) { color_ = color; }

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

		bool IsHideWhenBehindCamera() const { return hideWhenBehindCamera_; }
		void SetHideWhenBehindCamera(bool hideWhenBehindCamera) { hideWhenBehindCamera_ = hideWhenBehindCamera; }

	private: /// ---------- 内部処理 ---------- ///

		bool UpdateScreenPosition(Vector2& outScreenPosition) const;
		void EnsureSprite();
		void ApplySpriteSettings(const Vector2& screenPosition);
		std::vector<ComponentProperty> CreateProperties();

	private: /// ---------- メンバ変数 ---------- ///

		std::string texturePath_ = "Effects/white.dds";
		Vector2 screenOffset_ = { 0.0f, -64.0f };
		Vector2 size_ = { 64.0f, 64.0f };
		Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		bool visible_ = true;
		bool hideWhenBehindCamera_ = true;

		std::unique_ptr<Sprite> sprite_;
		std::string loadedTexturePath_;
	};
}
