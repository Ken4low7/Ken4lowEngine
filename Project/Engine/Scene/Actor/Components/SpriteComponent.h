#pragma once
#include "ActorComponent.h"
#include "ComponentProperty.h"
#include "Sprite.h"
#include "Vector2.h"
#include "Vector4.h"

#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///   Actorに画面固定の2D Sprite表示を追加するComponentクラス
	/// -------------------------------------------------------------
	class SpriteComponent : public ActorComponent
	{
	public: /// ---------- コンストラクタ / デストラクタ ---------- ///

		~SpriteComponent() override;

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SpriteComponentの初期化処理。
		/// </summary>
		void Initialize() override;

		void Update(float deltaTime) override;

		/// <summary>
		/// SpriteComponentの通常描画処理。
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
		/// SpriteComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

		/// <summary>
		/// SpriteComponentの終了処理。
		/// </summary>
		void Finalize() override;

	public: /// ---------- JSONシリアライズ / デシリアライズ ---------- ///

		/// <summary>
		/// JSON保存・復元で使用するComponentのクラス種別を取得する。
		/// </summary>
		std::string GetClassTypeName() const override
		{
			return "SpriteComponent"; // SpriteComponentとして保存する。
		}

		/// <summary>
		/// SpriteComponent固有情報をJSONへ保存する。
		/// </summary>
		void ToJson(nlohmann::json& outJson) const override;

		/// <summary>
		/// JSONからSpriteComponent固有情報を復元する。
		/// </summary>
		void FromJson(const nlohmann::json& inJson) override;

	public: /// ---------- 設定取得 ---------- ///

		const std::string& GetTexturePath() const { return texturePath_; }
		void SetTexturePath(const std::string& texturePath);

		const Vector2& GetPosition() const { return position_; }
		void SetPosition(const Vector2& position) { position_ = position; }

		const Vector2& GetSize() const { return size_; }
		void SetSize(const Vector2& size);

		const Vector4& GetColor() const { return color_; }
		void SetColor(const Vector4& color) { color_ = color; }

		float GetRotation() const { return rotation_; }
		void SetRotation(float rotation) { rotation_ = rotation; }

		const Vector2& GetAnchor() const { return anchor_; }
		void SetAnchor(const Vector2& anchor);

		bool IsVisible() const { return visible_; }
		void SetVisible(bool visible) { visible_ = visible; }

	private: /// ---------- 内部処理 ---------- ///

		void EnsureSprite();
		void ApplySpriteSettings();
		std::vector<ComponentProperty> CreateProperties();

	private: /// ---------- メンバ変数 ---------- ///

		std::string texturePath_ = "Effects/white.dds";
		Vector2 position_ = { 100.0f, 100.0f };
		Vector2 size_ = { 128.0f, 128.0f };
		Vector4 color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
		float rotation_ = 0.0f;
		Vector2 anchor_ = { 0.5f, 0.5f };
		bool visible_ = true;

		std::unique_ptr<Sprite> sprite_;
		std::string loadedTexturePath_;
	};
}
