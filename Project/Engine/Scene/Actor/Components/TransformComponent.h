#pragma once
#include "ActorComponent.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///		Actorの位置・回転・スケールを管理するComponentクラス
	/// -------------------------------------------------------------
	class TransformComponent : public ActorComponent
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// TransformComponentのImGui描画処理。
		/// </summary>
		void DrawImGui() override;

	public: /// ---------- ゲッター ---------- ///

		/// <summary>
		/// Actorのワールド位置を取得
		/// </summary>
		const Vector3& GetPosition() const { return position_; }

		/// <summary>
		/// Actorの回転角を取得
		/// </summary>
		const Vector3& GetRotation() const { return rotation_; }

		/// <summary>
		/// Actorの拡大率を取得
		/// </summary>
		const Vector3& GetScale() const { return scale_; }

	public: /// ---------- セッター ---------- ///

		/// <summary>
		/// Actorのワールド位置を設定
		/// </summary>
		void SetPosition(const Vector3& position) { position_ = position; }

		/// <summary>
		/// Actorの回転角を設定
		/// </summary>
		void SetRotation(const Vector3& rotation) { rotation_ = rotation; }

		/// <summary>
		/// Actorの拡大率を設定
		/// </summary>
		void SetScale(const Vector3& scale) { scale_ = scale; }

	private: /// ---------- メンバ変数 ---------- ///

		Vector3 position_{ 0.0f, 0.0f, 0.0f }; // Actorのワールド位置。
		Vector3 rotation_{ 0.0f, 0.0f, 0.0f }; // Actorの回転角。
		Vector3 scale_{ 1.0f, 1.0f, 1.0f };    // Actorの拡大率。
	};
}