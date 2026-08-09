#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "FontRasterizer.h"

/// <summary>
/// .ttf / .otf などのフォントファイルから、ゲーム内で使う文字だけをアトラス画像とJSONへ変換するクラス
/// 変換の流れは「オプション解析 → 文字一覧の決定 → 文字ラスタライズ → アトラス化 → PNG/JSON出力」
/// </summary>
class FontConverter
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// コマンドライン引数の使い方をコンソールへ表示
	/// </summary>
	static void OutputUsage();

	/// <summary>
	/// 指定されたフォントファイルを読み込み、使用文字一覧をアトラス画像とメタデータJSONへ変換
	/// </summary>
	/// <param name="filePath">変換対象のフォントファイルパス</param>
	/// <param name="numOptions">オプション引数の数。</param>
	/// <param name="options">-size、-out、-charsetFile などのオプション配列</param>
	/// <returns>変換に成功した場合は true</returns>
	bool ConvertFont(const std::wstring& filePath, int numOptions, wchar_t** options);

private: /// ---------- ヘルパー関数 ---------- ///

	/// <summary>
	/// コマンドラインオプションを解析し、フォントサイズや出力先などの設定値へ反映
	/// </summary>
	void ParseOptions(int numOptions, wchar_t** options);

	/// <summary>
	/// 文字一覧が指定されなかった場合に、英数字と記号中心の既定文字一覧を作成
	/// </summary>
	void BuildDefaultCharset();

	/// <summary>
	/// UTF-8の文字一覧ファイルを読み込み、変換対象の文字集合として保持
	/// </summary>
	bool LoadCharsetFromFile(const std::wstring& filePath);

	/// <summary>
	/// UTF-8テキストファイルを読み込み、Wide文字列へ変換
	/// </summary>
	std::wstring ReadTextFileUtf8(const std::wstring& filePath) const;

	/// <summary>
	/// TextSpriteDrawerが参照するための、文字ごとのUV・サイズ・送り幅をJSONとして保存
	/// </summary>
	void SaveMetadataJson(const std::wstring& outputPath, const std::wstring& inputFilePath, const FontAtlas& atlas) const;

	/// <summary>
	/// 変換設定の確認用に、簡易的なテキストファイルを出力
	/// </summary>
	void WriteDummyOutput(const std::wstring& outputPath, const std::wstring& inputFilePath) const;

	/// <summary>
	/// 先頭グリフの確認用として、1文字分のビットマップをPGM形式で保存
	/// </summary>
	void SaveGlyphBitmapAsPgm(const std::wstring& outputPath, const RasterizedGlyph& glyph) const;

	/// <summary>
	/// アトラス全体のアルファ画像をPGM形式で保存
	/// </summary>
	void SaveAtlasAsPgm(const std::wstring& outputPath, const FontAtlas& atlas) const;

	/// <summary>
	/// アルファ画像を白色RGBAのPNGとして保存
	/// </summary>
	void SaveAlphaImageAsPng(const std::wstring& outputPath, int width, int height, const std::vector<std::uint8_t>& alphaPixels) const;

	/// <summary>
	/// 入力フォントファイル名から、出力ファイル名のベースになる名前を作成
	/// </summary>
	std::wstring MakeOutputBaseName(const std::wstring& inputFilePath) const;

	/// <summary>
	/// JSONへ書き込むため、Wide文字列をUTF-8文字列へ変換
	/// </summary>
	std::string NarrowForJson(const std::wstring& text) const;

	/// <summary>
	/// 使用文字一覧に含まれる文字を1文字ずつラスタライズ
	/// </summary>
	std::vector<RasterizedGlyph> RasterizeCharset() const;

	/// <summary>
	/// ラスタライズ済み文字を、横方向に並べてから必要に応じて改行し、1枚のアトラスへまとめる
	/// </summary>
	FontAtlas BuildSimpleHorizontalAtlas(std::vector<RasterizedGlyph> glyphs) const;

private: /// ---------- メンバ変数 ---------- ///

	// ラスタライズするフォントサイズ
	int fontSize_ = 48;

	// アトラス画像の最大幅
	int atlasWidth_ = 1024;

	// 将来的な固定高さ指定用の値です。現在のアトラス生成では必要な高さを自動計算する
	int atlasHeight_ = 1024;

	// PNG / JSON の出力先ディレクトリ
	std::wstring outputDirectory_ = L"Resources/Fonts/Compiled/";

	// アトラス化する使用文字一覧
	std::wstring charset_;

	// メインフォントに存在しない文字を補うためのフォールバックフォントパス
	std::wstring fallbackFontPath_;

	// メインフォント用のラスタライザ
	FontRasterizer fontRasterizer_;

	// フォールバックフォント用のラスタライザ
	FontRasterizer fallbackFontRasterizer_;
};
