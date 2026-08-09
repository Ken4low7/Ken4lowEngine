#pragma once
#include <memory>
#include <array>
#include <string>
#include "Sprite.h"
#include "Vector2.h"
#include <vector>

namespace Ken4lowEngine
{


/// --------------------------------------------------------------
///				　	数字スプライト描画クラス
/// --------------------------------------------------------------
class NumberSpriteDrawer
{
public: /// ---------- メンバ関数 ---------------- ///

	/// <summary>
	/// 数字スプライト描画クラスの初期化
	/// </summary>
	/// <param name="texturePath">テクスチャパス</param>
	/// <param name="digitWidth">テクスチャの幅</param>
	/// <param name="digitHeight">テクスチャの高さ</param>
	/// <param name="drawDigitWidth">画面に描く幅（-1でテクスチャサイズに合わせる）</param>
	/// <param name="drawDigitHeight">画面に描く高さ（-1でテクスチャサイズに合わせる）</param>
	void Initialize(const std::string& texturePath,
		float srcDigitWidth = 50.0f, float srcDigitHeight = 50.0f,
		float drawDigitWidth = -1.0f, float drawDigitHeight = -1.0f);

	/// <summary> 画面に描くサイズだけ変更（切り出しサイズは固定のまま） </summary>
	void SetDrawDigitSize(float drawDigitWidth, float drawDigitHeight);

	/// <summary>
	/// 左詰めで数字を描画
	/// </summary>
	/// <param name="value">数字</param>
	/// <param name="position">座標</param>
	/// <param name="spacing">桁間のスペース</param>
	void DrawNumberLeftAligned(int value, const Vector2& position, float spacing = 24.0f);

	/// <summary>
	/// 中央揃えで数字を描画
	/// </summary>
	/// <param name="value">数字</param>
	/// <param name="centerPosition">座標</param>
	/// <param name="spacing">桁間のスペース</param>
	void DrawNumberCentered(int value, const Vector2& centerPosition, float spacing = 24.0f);

	/// <summary>
	/// 右詰めで数字を描画
	/// </summary>
	/// <param name="value">数字</param>
	/// <param name="rightPosition">座標</param>
	/// <param name="spacing">桁間のスペース</param>
	void DrawNumberRightAligned(int value, Vector2 rightPosition, float spacing = 24.0f);

	/// <summary>
	/// インデックスをリセット
	/// </summary>
	void Reset() { currentIndex_ = 0; }

private: /// ---------- メンバ変数 ---------------- ///

	std::string texturePath_; // テクスチャパス

	float srcW_ = 50.0f;  //
	float srcH_ = 50.0f;  //
	float drawW_ = 50.0f; //
	float drawH_ = 50.0f; //

	std::vector<std::unique_ptr<Sprite>> reusable_;
	size_t currentIndex_ = 0;
};

} // namespace Ken4lowEngine
