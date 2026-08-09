#pragma once
#include "Vector2.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// 画面解像度と基準解像度の変換を管理するクラス。
	/// -------------------------------------------------------------
	class ResolutionManager
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// ResolutionManager のシングルトンインスタンスを取得する。
		/// </summary>
		static ResolutionManager* GetInstance();

		/// <summary>
		/// 現在の画面サイズを設定する。
		/// </summary>
		void SetScreenSize(float width, float height);

		/// <summary>
		/// 基準解像度上の座標を現在の画面座標へ変換する。
		/// </summary>
		Vector2 ToScreen(const Vector2& logicalPos) const;

		/// <summary>
		/// 現在の画面座標を基準解像度上の座標へ変換する。
		/// </summary>
		Vector2 ToLogical(const Vector2& screenPos) const;

	public: /// ---------- ゲッター ---------- ///

		/// <summary>
		/// 現在の画面幅を取得する。
		/// </summary>
		float GetScreenWidth() const { return screenWidth_; }

		/// <summary>
		/// 現在の画面高さを取得する。
		/// </summary>
		float GetScreenHeight() const { return screenHeight_; }

		/// <summary>
		/// 横方向のスケールを取得する。
		/// </summary>
		float GetScaleX() const { return screenWidth_ / kBaseWidth; }

		/// <summary>
		/// 縦方向のスケールを取得する。
		/// </summary>
		float GetScaleY() const { return screenHeight_ / kBaseHeight; }

		/// <summary>
		/// 現在のアスペクト比を取得する。
		/// </summary>
		float GetAspectRatio() const { return screenWidth_ / screenHeight_; }

	private: /// ---------- メンバ変数 ---------- ///

		static constexpr float kBaseWidth = 1920.0f;  // 基準解像度の幅。これを基準に座標変換を行います。
		static constexpr float kBaseHeight = 1080.0f; // 基準解像度の高さ。これを基準に座標変換を行います。

		float screenWidth_ = kBaseWidth;   // 現在の画面幅。初期値は基準解像度と同じにします。
		float screenHeight_ = kBaseHeight; // 現在の画面高さ。初期値は基準解像度と同じにします。
	};
} // namespace Ken4lowEngine
