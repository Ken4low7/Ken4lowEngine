#pragma once
#include <cstdint>
#include <vector>

/// <summary>
/// 1文字分の描画情報
/// TextSpriteDrawer側では、ここに保存されたUV座標・サイズ・送り幅を参照して文字を配置
/// </summary>
struct GlyphInfo
{
	/// <summary>
	/// Unicodeコードポイントです。例: 'A' は U+0041、'あ' は U+3042
	/// </summary>
	uint32_t codepoint = 0;

	/// <summary>
	/// アトラス画像上の左上UV座標
	/// </summary>
	float u0 = 0.0f;
	float v0 = 0.0f;

	/// <summary>
	///	アトラス画像上の右下UV座標
	/// </summary>
	float u1 = 0.0f;
	float v1 = 0.0f;

	/// <summary>
	/// 描画時に使う文字の幅
	/// </summary>
	float width = 0.0f;

	/// <summary>
	/// 描画時に使う文字の高さ
	/// </summary>
	float height = 0.0f;

	/// <summary>
	/// 文字の左側の余白です。文字の位置合わせに使う
	/// </summary>
	float bearingX = 0.0f;

	/// <summary>
	/// ベースラインから文字上端までの距離、日本語と英数字の高さを揃えるために使う
	/// </summary>
	float bearingY = 0.0f;

	/// <summary>
	/// この文字を描画した後、次の文字へ進める横方向の距離
	/// </summary>
	float advanceX = 0.0f;
};

/// <summary>
/// DirectWriteでラスタライズした1文字分のビットマップと、その文字情報をまとめた構造体
/// </summary>
struct RasterizedGlyph
{
	/// <summary>
	/// 描画側へ渡す1文字分のメタデータ
	/// </summary>
	GlyphInfo glyphInfo;

	/// <summary>
	/// 1文字分のアルファ画像。0が透明、255が不透明
	/// </summary>
	std::vector<std::uint8_t> pixels;

	/// <summary>
	/// 1文字分のビットマップ幅
	/// </summary>
	int bitmapWidth = 0;

	/// <summary>
	/// 1文字分のビットマップ高さ
	/// </summary>
	int bitmapHeight = 0;

	/// <summary>
	/// アトラス画像内に配置された左上X座標
	/// </summary>
	int atlasX = 0;

	/// <summary>
	/// アトラス画像内に配置された左上Y座標
	/// </summary>
	int atlasY = 0;

	/// <summary>
	/// ラスタライズとアトラス配置に使える有効なグリフかどうか
	/// </summary>
	bool isValid = false;
};

/// <summary>
/// 複数文字を1枚の画像に詰め込んだフォントアトラス
/// PNG画像として保存し、JSONに各文字のUV情報を書き出す。
/// </summary>
struct FontAtlas
{
	/// <summary>
	/// アトラス画像の幅
	/// </summary>
	int width = 0;

	/// <summary>
	/// アトラス画像の高さ
	/// </summary>
	int height = 0;

	/// <summary>
	/// アトラス全体のアルファ画像
	/// </summary>
	std::vector<std::uint8_t> pixels;

	/// <summary>
	/// アトラスに登録された全文字の情報
	/// </summary>
	std::vector<RasterizedGlyph> glyphs;

	/// <summary>
	/// アトラス生成に成功したかどうか
	/// </summary>
	bool isValid = false;
};
