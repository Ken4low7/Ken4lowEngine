#pragma once
#include <string>
#include <wrl/client.h>
#include <dwrite.h>

#include "FontConverterTypes.h"

/// ---------- フォントファイルローダーの前方宣言 ---------- ///
class FontFileLoader;

/// <summary>
/// DirectWriteを使って、フォントファイル内の1文字をアルファビットマップへ変換するクラス
/// FontConverter本体から呼び出され、文字の有無確認・メトリクス取得・ビットマップ生成を担当
/// </summary>
class FontRasterizer
{
public: /// ---------- コンストラクタ・デストラクタ ---------- ///

	FontRasterizer() = default;
	~FontRasterizer();

public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DirectWriteのFactoryを生成し、独自フォントローダーを登録
	/// </summary>
	/// <returns></returns>
	bool Initialize();

	/// <summary>
	/// 指定されたフォントファイルをDirectWriteへ登録し、ラスタライズに使うFontFaceを作成
	/// </summary>
	/// <param name="filePath">ファイルパス</param>
	/// <returns></returns>
	bool LoadFontFile(const std::wstring& filePath);

	/// <summary>
	/// 指定された文字が現在のフォントに含まれているかを確認
	/// </summary>
	/// <param name="character">確認する文字</param>
	/// <returns></returns>
	bool HasGlyph(wchar_t character) const;

	/// <summary>
	/// 指定された文字をDirectWriteでラスタライズし、GlyphInfoとアルファビットマップを返す。
	/// </summary>
	/// <param name="character">ラスタライズする文字</param>
	/// <param name="fontSize">フォントサイズ</param>
	/// <returns></returns>
	RasterizedGlyph RasterizeGlyph(wchar_t character, float fontSize) const;

private: /// ---------- ヘルパー関数 ---------- ///

	/// <summary>
	/// フォントに存在しない文字の代替表示用に、簡易的な白い矩形グリフを作成
	/// </summary>
	/// <param name="character">文字</param>
	/// <param name="fontSize">フォントサイズ</param>
	/// <returns></returns>
	RasterizedGlyph BuildFallbackGlyph(wchar_t character, float fontSize) const;

	/// <summary>
	/// DirectWriteのGlyphRunAnalysisを使い、1文字分のアルファビットマップを作成
	/// </summary>
	/// <param name="glyphIndex">グリフインデックス</param>
	/// <param name="fontSize">フォントサイズ</param>
	/// <param name="glyphMetrics">グリフメトリクス</param>
	/// <param name="fontMetrics">フォントメトリクス</param>
	/// <param name="outGlyph">出力グリフ</param>
	/// <returns></returns>
	bool BuildGlyphBitmap(UINT16 glyphIndex, float fontSize, const DWRITE_GLYPH_METRICS& glyphMetrics, const DWRITE_FONT_METRICS& fontMetrics, RasterizedGlyph& outGlyph) const;

private: /// ---------- メンバ変数 ---------- ///

	// DirectWriteの各種オブジェクトを生成するFactory
	Microsoft::WRL::ComPtr<IDWriteFactory> dwriteFactory_;

	// 現在読み込んでいるフォントファイルパス
	std::wstring loadedFontPath_;

	// 任意パスのフォントファイルをDirectWriteへ渡すための独自ローダー
	IDWriteFontFileLoader* fontFileLoader_ = nullptr;

	// DirectWriteが参照するフォントファイルオブジェクト
	Microsoft::WRL::ComPtr<IDWriteFontFile> fontFile_;
	
	// グリフのメトリクス取得やビットマップ化に使うフォントフェイスです
	Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace_;
};