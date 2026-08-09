#include "FontConverter.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")

namespace
{
	/// <summary>
	/// JSON文字列として壊れないよう、引用符・バックスラッシュ・改行などをエスケープする
	/// </summary>
	std::string EscapeJsonString(const std::string& s)
	{
		std::string out;
		out.reserve(s.size());

		for (char c : s)
		{
			switch (c)
			{
			case '\"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\n': out += "\\n";  break;
			case '\r': out += "\\r";  break;
			case '\t': out += "\\t";  break;
			default:   out += c;      break;
			}
		}

		return out;
	}
}

void FontConverter::OutputUsage()
{
	// コマンドライン実行時に必要な引数と、指定可能なオプションを表示する。
	std::wcout << L"Usage:\n";
	std::wcout << L"  FontConverter.exe <FontFilePath> [options]\n";
	std::wcout << L"\nOptions:\n";
	std::wcout << L"  -size <int>              Font size\n";
	std::wcout << L"  -out <dir>               Output directory\n";
	std::wcout << L"  -atlasWidth <int>        Atlas width\n";
	std::wcout << L"  -atlasHeight <int>       Atlas height\n";
	std::wcout << L"  -charset <string>        Characters to bake\n";
	std::wcout << L"  -charsetFile <path>      UTF-8 text file containing characters to bake\n";
	std::wcout << L"  -fallbackFont <path>     Fallback font file path\n";
}

bool FontConverter::ConvertFont(const std::wstring& filePath, int numOptions, wchar_t** options)
{
	// 先にオプションを解析し、フォントサイズ・出力先・文字一覧などを確定する。
	ParseOptions(numOptions, options);

	// 入力フォントが存在しない場合は、以降のDirectWrite処理に進まず終了する。
	if (!std::filesystem::exists(filePath))
	{
		std::wcerr << L"[FontConverter] Input file not found: " << filePath << L"\n";
		return false;
	}

	if (charset_.empty())
	{
		// 使用文字一覧が指定されていない場合は、最低限の英数字・記号を対象にする。
		BuildDefaultCharset();
	}

	std::error_code ec;
	// PNGやJSONを書き込めるよう、出力先ディレクトリを作成する。
	std::filesystem::create_directories(outputDirectory_, ec);
	if (ec)
	{
		std::wcerr << L"[FontConverter] Failed to create output directory: "
			<< outputDirectory_ << L"\n";
		return false;;
	}

	std::wcout << L"[FontConverter] Start\n";
	std::wcout << L"  Input  : " << filePath << L"\n";
	std::wcout << L"  Size   : " << fontSize_ << L"\n";
	std::wcout << L"  Atlas  : " << atlasWidth_ << L"x" << atlasHeight_ << L"\n";
	std::wcout << L"  Output : " << outputDirectory_ << L"\n";
	std::wcout << L"  Charset length : " << charset_.size() << L"\n";

	// メインフォント用のDirectWriteラスタライザを初期化する。
	if (!fontRasterizer_.Initialize())
	{
		std::wcerr << L"[FontConverter] Failed to initialize FontRasterizer.\n";
		return false;
	}

	// 入力フォントファイルを読み込み、グリフ取得に使うFontFaceを作成する。
	if (!fontRasterizer_.LoadFontFile(filePath))
	{
		std::wcerr << L"[FontConverter] Failed to load font file into FontRasterizer.\n";
		return false;
	}

	bool fallbackEnabled = false;

	if (!fallbackFontPath_.empty())
	{
		// メインフォントに存在しない文字を補うため、任意指定のフォールバックフォントを準備する。
		std::wcout << L"[FontConverter] Try load fallback font: " << fallbackFontPath_ << L"\n";

		if (!std::filesystem::exists(fallbackFontPath_))
		{
			std::wcerr << L"[FontConverter] Fallback font not found: " << fallbackFontPath_ << L"\n";
		}
		else
		{
			if (!fallbackFontRasterizer_.Initialize())
			{
				std::wcerr << L"[FontConverter] Failed to initialize fallback FontRasterizer.\n";
			}
			else if (!fallbackFontRasterizer_.LoadFontFile(fallbackFontPath_))
			{
				std::wcerr << L"[FontConverter] Failed to load fallback font: " << fallbackFontPath_ << L"\n";
			}
			else
			{
				fallbackEnabled = true;
				std::wcout << L"[FontConverter] Fallback font loaded successfully: "
					<< fallbackFontPath_ << L"\n";
			}
		}
	}
	else
	{
		std::wcout << L"[FontConverter] No fallback font specified.\n";
	}

	if (fallbackEnabled)
	{
		std::wcout << L"[FontConverter] Fallback test '/' : "
			<< (fallbackFontRasterizer_.HasGlyph(L'/') ? L"OK" : L"NG") << L"\n";
		std::wcout << L"[FontConverter] Fallback test 'あ' : "
			<< (fallbackFontRasterizer_.HasGlyph(L'あ') ? L"OK" : L"NG") << L"\n";
	}

	// 使用文字一覧を1文字ずつビットマップ化する。
	std::vector<RasterizedGlyph> glyphs = RasterizeCharset();
	if (glyphs.empty())
	{
		std::wcerr << L"[FontConverter] No valid glyphs were rasterized.\n";
		return false;
	}

	// ラスタライズした全文字を1枚のアトラス画像にまとめる。
	FontAtlas atlas = BuildSimpleHorizontalAtlas(std::move(glyphs));
	if (!atlas.isValid)
	{
		std::wcerr << L"[FontConverter] Failed to build atlas.\n";
		return false;
	}

	// 入力フォント名を元に、出力ファイル名の共通部分を作成する。
	const std::wstring baseName = MakeOutputBaseName(filePath);

	const std::filesystem::path dummyPath =
		std::filesystem::path(outputDirectory_) / (baseName + L"_dummy.txt");

	const std::filesystem::path jsonPath =
		std::filesystem::path(outputDirectory_) / (baseName + L".json");

	const std::filesystem::path atlasPgmPath =
		std::filesystem::path(outputDirectory_) / (baseName + L"_atlas.pgm");

	const std::filesystem::path glyphBitmapPath =
		std::filesystem::path(outputDirectory_) / (baseName + L"_A.pgm");

	const std::filesystem::path atlasPngPath =
		std::filesystem::path(outputDirectory_) / (baseName + L"_atlas.png");

	const std::filesystem::path glyphPngPath =
		std::filesystem::path(outputDirectory_) / (baseName + L"_A.png");

	// 確認用テキスト、描画用JSON、アトラス画像をそれぞれ出力する。
	WriteDummyOutput(dummyPath.wstring(), filePath);
	SaveMetadataJson(jsonPath.wstring(), filePath, atlas);
	SaveAtlasAsPgm(atlasPgmPath.wstring(), atlas);
	SaveGlyphBitmapAsPgm(glyphBitmapPath.wstring(), atlas.glyphs.front());

	SaveAlphaImageAsPng(
		atlasPngPath.wstring(),
		atlas.width,
		atlas.height,
		atlas.pixels
	);

	SaveAlphaImageAsPng(
		glyphPngPath.wstring(),
		atlas.glyphs.front().bitmapWidth,
		atlas.glyphs.front().bitmapHeight,
		atlas.glyphs.front().pixels
	);

	std::wcout << L"[FontConverter] Wrote dummy file : " << dummyPath.wstring() << L"\n";
	std::wcout << L"[FontConverter] Wrote json file  : " << jsonPath.wstring() << L"\n";
	std::wcout << L"[FontConverter] Wrote atlas pgm  : " << atlasPgmPath.wstring() << L"\n";
	std::wcout << L"[FontConverter] Wrote glyph pgm  : " << glyphBitmapPath.wstring() << L"\n";
	std::wcout << L"[FontConverter] Wrote atlas png  : " << atlasPngPath.wstring() << L"\n";
	std::wcout << L"[FontConverter] Wrote glyph png  : " << glyphPngPath.wstring() << L"\n";
	std::wcout << L"[FontConverter] Glyph count      : " << atlas.glyphs.size() << L"\n";

	return true;
}

void FontConverter::ParseOptions(int numOptions, wchar_t** options)
{
	for (int i = 0; i < numOptions; ++i)
	{
		// オプション名と次の値をセットで読み取り、対応する設定へ反映する。
		const std::wstring arg = options[i];

		if (arg == L"-size" && i + 1 < numOptions)
		{
			fontSize_ = std::stoi(options[++i]);
		}
		else if (arg == L"-out" && i + 1 < numOptions)
		{
			outputDirectory_ = options[++i];
		}
		else if (arg == L"-atlasWidth" && i + 1 < numOptions)
		{
			atlasWidth_ = std::stoi(options[++i]);
		}
		else if (arg == L"-atlasHeight" && i + 1 < numOptions)
		{
			atlasHeight_ = std::stoi(options[++i]);
		}
		else if (arg == L"-charset" && i + 1 < numOptions)
		{
			charset_ = options[++i];
		}
		else if (arg == L"-charsetFile" && i + 1 < numOptions)
		{
			const std::wstring charsetFilePath = options[++i];
			if (!LoadCharsetFromFile(charsetFilePath))
			{
				std::wcerr << L"[FontConverter] Failed to load charset file: " << charsetFilePath << L"\n";
			}
		}
		else if (arg == L"-fallbackFont" && i + 1 < numOptions)
		{
			fallbackFontPath_ = options[++i];
		}
		else
		{
			std::wcerr << L"[FontConverter] Warning: Unknown or incomplete option: "
				<< arg << L"\n";
		}
	}
}

void FontConverter::BuildDefaultCharset()
{
	// charsetFileが指定されていない場合でも最低限のUI文字を出せるよう、既定文字を用意する。
	charset_ =
		L"0123456789"
		L"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		L"abcdefghijklmnopqrstuvwxyz"
		L"!\"#$%&'()=-^~\\|@`[{;+:*]},<.>/?_ ";
}

std::wstring FontConverter::ReadTextFileUtf8(const std::wstring& filePath) const
{
	// UTF-8 BOMの有無を確認できるよう、バイナリとして読み込む。
	std::ifstream ifs(std::filesystem::path(filePath), std::ios::binary);
	if (!ifs)
	{
		return L"";
	}

	std::string bytes(
		(std::istreambuf_iterator<char>(ifs)),
		std::istreambuf_iterator<char>()
	);

	if (bytes.empty())
	{
		return L"";
	}

	// UTF-8 BOM 除去
	if (bytes.size() >= 3 &&
		static_cast<unsigned char>(bytes[0]) == 0xEF &&
		static_cast<unsigned char>(bytes[1]) == 0xBB &&
		static_cast<unsigned char>(bytes[2]) == 0xBF)
	{
		bytes.erase(0, 3);
	}

	// UTF-8のバイト列をWide文字列へ変換するため、まず必要な文字数を取得する。
	const int size = // 確保したバッファへ、実際にUTF-8からWide文字列へ変換する。
	MultiByteToWideChar(
		CP_UTF8,
		0,
		bytes.data(),
		static_cast<int>(bytes.size()),
		nullptr,
		0
	);

	if (size <= 0)
	{
		return L"";
	}

	std::wstring result(static_cast<size_t>(size), L'\0');

	// 確保したバッファへ、実際にUTF-8からWide文字列へ変換する。
	MultiByteToWideChar(
		CP_UTF8,
		0,
		bytes.data(),
		static_cast<int>(bytes.size()),
		result.data(),
		size
	);

	return result;
}

bool FontConverter::LoadCharsetFromFile(const std::wstring& filePath)
{
	const std::wstring text = ReadTextFileUtf8(filePath);
	if (text.empty())
	{
		return false;
	}

	// 文字一覧ファイルの内容を、実際にアトラス化する文字集合として保存する。
	charset_.clear();
	charset_.reserve(text.size());

	for (wchar_t ch : text)
	{
		// 改行・タブは除外
		if (ch == L'\r' || ch == L'\n' || ch == L'\t')
		{
			continue;
		}

		charset_.push_back(ch);
	}

	std::wcout << L"[FontConverter] Loaded charset file: " << filePath
		<< L" (length=" << charset_.size() << L")\n";

	return !charset_.empty();
}

void FontConverter::SaveMetadataJson(
	const std::wstring& outputPath,
	const std::wstring& inputFilePath,
	const FontAtlas& atlas
) const
{
	// JSONは毎回作り直すため、既存ファイルを上書きする。
	std::ofstream ofs(std::filesystem::path(outputPath), std::ios::out | std::ios::trunc);
	if (!ofs)
	{
		std::wcerr << L"[FontConverter] Failed to create json file: " << outputPath << L"\n";
		return;
	}

	// WindowsのWide文字列パスを、JSONへ書けるUTF-8文字列に変換する。
	const std::string inputFileUtf8 = NarrowForJson(inputFilePath);
	const std::string charsetUtf8 = NarrowForJson(charset_);

	ofs << "{\n";
	ofs << "  \"inputFile\": \"" << EscapeJsonString(inputFileUtf8) << "\",\n";
	ofs << "  \"fontSize\": " << fontSize_ << ",\n";
	ofs << "  \"atlasWidth\": " << atlas.width << ",\n";
	ofs << "  \"atlasHeight\": " << atlas.height << ",\n";
	ofs << "  \"charsetLength\": " << charset_.size() << ",\n";
	ofs << "  \"charsetPreview\": \"" << EscapeJsonString(charsetUtf8) << "\",\n";
	ofs << "  \"glyphCount\": " << atlas.glyphs.size() << ",\n";
	ofs << "  \"glyphs\": [\n";

	for (size_t i = 0; i < atlas.glyphs.size(); ++i)
	{
		// TextSpriteDrawerが1文字ずつ描画できるよう、各グリフのUVとメトリクスを書き出す。
		const auto& g = atlas.glyphs[i];
		ofs << "    {\n";
		ofs << "      \"codepoint\": " << g.glyphInfo.codepoint << ",\n";
		ofs << "      \"width\": " << g.glyphInfo.width << ",\n";
		ofs << "      \"height\": " << g.glyphInfo.height << ",\n";
		ofs << "      \"bearingX\": " << g.glyphInfo.bearingX << ",\n";
		ofs << "      \"bearingY\": " << g.glyphInfo.bearingY << ",\n";
		ofs << "      \"advanceX\": " << g.glyphInfo.advanceX << ",\n";
		ofs << "      \"bitmapWidth\": " << g.bitmapWidth << ",\n";
		ofs << "      \"bitmapHeight\": " << g.bitmapHeight << ",\n";
		ofs << "      \"atlasX\": " << g.atlasX << ",\n";
		ofs << "      \"atlasY\": " << g.atlasY << ",\n";
		ofs << "      \"u0\": " << g.glyphInfo.u0 << ",\n";
		ofs << "      \"v0\": " << g.glyphInfo.v0 << ",\n";
		ofs << "      \"u1\": " << g.glyphInfo.u1 << ",\n";
		ofs << "      \"v1\": " << g.glyphInfo.v1 << "\n";
		ofs << "    }";
		if (i + 1 < atlas.glyphs.size())
		{
			ofs << ",";
		}
		ofs << "\n";
	}

	ofs << "  ],\n";
	ofs << "  \"note\": \"Temporary horizontal atlas build before real packing is implemented.\"\n";
	ofs << "}\n";
}

void FontConverter::WriteDummyOutput(const std::wstring& outputPath, const std::wstring& inputFilePath) const
{
	std::ofstream ofs(std::filesystem::path(outputPath), std::ios::out | std::ios::trunc);
	if (!ofs)
	{
		std::wcerr << L"[FontConverter] Failed to create dummy output file: "
			<< outputPath << L"\n";
		return;
	}

	ofs << "FontConverter output test\n";
	ofs << "InputFile    : " << NarrowForJson(inputFilePath) << "\n";
	ofs << "FontSize     : " << fontSize_ << "\n";
	ofs << "AtlasSize    : " << atlasWidth_ << " x " << atlasHeight_ << "\n";
	ofs << "OutputDir    : " << NarrowForJson(outputDirectory_) << "\n";
	ofs << "CharsetLength: " << charset_.size() << "\n";
}

void FontConverter::SaveGlyphBitmapAsPgm(const std::wstring& outputPath, const RasterizedGlyph& glyph) const
{
	if (!glyph.isValid)
	{
		std::wcerr << L"[FontConverter] Glyph is invalid. Skip pgm output.\n";
		return;
	}

	if (glyph.bitmapWidth <= 0 || glyph.bitmapHeight <= 0)
	{
		std::wcerr << L"[FontConverter] Invalid bitmap size. Skip pgm output.\n";
		return;
	}

	if (glyph.pixels.empty())
	{
		std::wcerr << L"[FontConverter] Glyph pixels are empty. Skip pgm output.\n";
		return;
	}

	std::ofstream ofs(std::filesystem::path(outputPath), std::ios::binary | std::ios::trunc);
	if (!ofs)
	{
		std::wcerr << L"[FontConverter] Failed to create glyph pgm file: " << outputPath << L"\n";
		return;
	}

	// PGM(P5)は単純なグレースケール形式なので、デバッグ用にアルファ画像を確認しやすい。
	ofs << "P5\n";
	ofs << glyph.bitmapWidth << " " << glyph.bitmapHeight << "\n";
	ofs << "255\n";
	ofs.write(
		reinterpret_cast<const char*>(glyph.pixels.data()),
		static_cast<std::streamsize>(glyph.pixels.size())
	);

	if (!ofs.good())
	{
		std::wcerr << L"[FontConverter] Failed while writing glyph pgm file: " << outputPath << L"\n";
		return;
	}
}

std::wstring FontConverter::MakeOutputBaseName(const std::wstring& inputFilePath) const
{
	std::filesystem::path p(inputFilePath);
	return p.stem().wstring();
}

std::string FontConverter::NarrowForJson(const std::wstring& text) const
{
	if (text.empty())
	{
		return {};
	}

	// JSON出力用に、Wide文字列をUTF-8へ変換するための必要バイト数を取得する。
	const int size = // 確保したバッファへ、実際にWide文字列からUTF-8へ変換する。
	WideCharToMultiByte(
		CP_UTF8,
		0,
		text.c_str(),
		-1,
		nullptr,
		0,
		nullptr,
		nullptr
	);

	if (size <= 0)
	{
		return "[WideCharToMultiByte failed]";
	}

	std::string result(static_cast<size_t>(size), '\0');

	// 確保したバッファへ、実際にWide文字列からUTF-8へ変換する。
	WideCharToMultiByte(
		CP_UTF8,
		0,
		text.c_str(),
		-1,
		result.data(),
		size,
		nullptr,
		nullptr
	);

	if (!result.empty() && result.back() == '\0')
	{
		result.pop_back();
	}

	return result;
}

std::vector<RasterizedGlyph> FontConverter::RasterizeCharset() const
{
	std::vector<RasterizedGlyph> glyphs;
	glyphs.reserve(charset_.size());

	for (wchar_t c : charset_)
	{
		// メインフォントに存在する文字はメインフォントから、なければフォールバックフォントから生成する。
		RasterizedGlyph glyph{};

		if (fontRasterizer_.HasGlyph(c))
		{
			std::wcout << L"[FontConverter] Use main font for U+"
				<< std::hex << static_cast<std::uint32_t>(c) << std::dec
				<< L" ('" << c << L"')\n";

			glyph = fontRasterizer_.RasterizeGlyph(c, static_cast<float>(fontSize_));
		}
		else if (!fallbackFontPath_.empty() && fallbackFontRasterizer_.HasGlyph(c))
		{
			std::wcout << L"[FontConverter] Use fallback font for U+"
				<< std::hex << static_cast<std::uint32_t>(c) << std::dec
				<< L" ('" << c << L"')\n";

			glyph = fallbackFontRasterizer_.RasterizeGlyph(c, static_cast<float>(fontSize_));
		}
		else
		{
			std::wcout << L"[FontConverter] Missing glyph in all fonts: U+"
				<< std::hex << static_cast<std::uint32_t>(c) << std::dec
				<< L" ('" << c << L"') -> skipped\n";
			continue;
		}

		if (!glyph.isValid)
		{
			std::wcout << L"[FontConverter] Glyph rasterize failed: U+"
				<< std::hex << static_cast<std::uint32_t>(c) << std::dec
				<< L" ('" << c << L"') -> skipped\n";
			continue;
		}

		// 有効なグリフだけをアトラス生成対象に追加する。
		glyphs.push_back(std::move(glyph));
	}

	return glyphs;
}

FontAtlas FontConverter::BuildSimpleHorizontalAtlas(std::vector<RasterizedGlyph> glyphs) const
{
	FontAtlas atlas{};

	if (glyphs.empty())
	{
		return atlas;
	}

	// 文字同士がにじんで見えないよう、各グリフの周囲に余白を入れる。
	constexpr int kPadding = 4;
	const int maxAtlasWidth = atlasWidth_;

	if (maxAtlasWidth <= 0)
	{
		return atlas;
	}

	// 1行に配置するグリフ範囲と、ベースライン計算に必要な情報を保持する。
	struct RowInfo
	{
		int startIndex = 0;
		int endIndex = 0; // exclusive
		float maxBearingY = 0.0f;
		float maxDescender = 0.0f;
		int rowWidth = 0;
		int rowHeight = 0;
		int baselineY = 0;
		int y = 0;
	};

	std::vector<RowInfo> rows;
	rows.reserve(64);

	// -----------------------------
	// 1. 行分割：横幅を超えそうになったら次の行へ送る
	// -----------------------------
	int rowStart = 0;
	int cursorX = kPadding;

	float rowMaxBearingY = 0.0f;
	float rowMaxDescender = 0.0f;

	for (int i = 0; i < static_cast<int>(glyphs.size()); ++i)
	{
		const auto& glyph = glyphs[i];
		const int glyphW = glyph.bitmapWidth;
		const int glyphH = glyph.bitmapHeight;

		if (glyphW <= 0 || glyphH <= 0)
		{
			continue;
		}

		// 現在の行にこのグリフを置いた場合、アトラス幅を超えるかを計算する。
		const int requiredWidth = (cursorX == kPadding)
			? (glyphW + kPadding)
			: (cursorX + glyphW + kPadding);

		// はみ出すならここで改行
		if (cursorX != kPadding && requiredWidth > maxAtlasWidth)
		{
			RowInfo row{};
			row.startIndex = rowStart;
			row.endIndex = i;
			row.maxBearingY = rowMaxBearingY;
			row.maxDescender = rowMaxDescender;
			row.rowWidth = cursorX;
			row.rowHeight = static_cast<int>(std::ceil(rowMaxBearingY + rowMaxDescender));
			rows.push_back(row);

			rowStart = i;
			cursorX = kPadding;
			rowMaxBearingY = 0.0f;
			rowMaxDescender = 0.0f;
		}

		cursorX += glyphW + kPadding;

		if (glyph.glyphInfo.bearingY > rowMaxBearingY)
		{
			rowMaxBearingY = glyph.glyphInfo.bearingY;
		}

		// ベースラインより下に出る量を求め、行の高さ計算に使う。
		const float descender =
			static_cast<float>(glyph.bitmapHeight) - glyph.glyphInfo.bearingY;

		if (descender > rowMaxDescender)
		{
			rowMaxDescender = descender;
		}
	}

	// 最終行
	if (rowStart < static_cast<int>(glyphs.size()))
	{
		RowInfo row{};
		row.startIndex = rowStart;
		row.endIndex = static_cast<int>(glyphs.size());
		row.maxBearingY = rowMaxBearingY;
		row.maxDescender = rowMaxDescender;
		row.rowWidth = cursorX;
		row.rowHeight = static_cast<int>(std::ceil(rowMaxBearingY + rowMaxDescender));
		rows.push_back(row);
	}

	if (rows.empty())
	{
		return atlas;
	}

	// -----------------------------
	// 2. 各行のY位置と baseline を決める：英数字と日本語の縦位置を揃える
	// -----------------------------
	int atlasHeightUsed = kPadding;
	int usedWidth = 0;

	for (auto& row : rows)
	{
		row.y = atlasHeightUsed;
		row.baselineY = row.y + static_cast<int>(std::ceil(row.maxBearingY));
		atlasHeightUsed += row.rowHeight + kPadding;

		if (row.rowWidth > usedWidth)
		{
			usedWidth = row.rowWidth;
		}
	}

	atlas.width = maxAtlasWidth;
	atlas.height = atlasHeightUsed;

	if (atlas.width <= 0 || atlas.height <= 0)
	{
		return atlas;
	}

	// 透明なアトラス画像を用意し、ここへ各グリフのアルファ画像をコピーしていく。
	atlas.pixels.assign(static_cast<size_t>(atlas.width * atlas.height), 0);

	// -----------------------------
	// 3. 各 glyph を配置：行ごとのベースラインに合わせてピクセルをコピーする
	// -----------------------------
	for (const auto& row : rows)
	{
		int rowCursorX = kPadding;

		for (int i = row.startIndex; i < row.endIndex; ++i)
		{
			auto& glyph = glyphs[i];

			// ベースラインを基準に配置することで、文字ごとの高さ差があっても自然に並ぶようにする。
			glyph.atlasX = rowCursorX;
			glyph.atlasY = row.baselineY - static_cast<int>(std::round(glyph.glyphInfo.bearingY));

			for (int y = 0; y < glyph.bitmapHeight; ++y)
			{
				for (int x = 0; x < glyph.bitmapWidth; ++x)
				{
					const int dstX = glyph.atlasX + x;
					const int dstY = glyph.atlasY + y;

					if (dstX < 0 || dstX >= atlas.width || dstY < 0 || dstY >= atlas.height)
					{
						continue;
					}

					atlas.pixels[static_cast<size_t>(dstY * atlas.width + dstX)] =
						glyph.pixels[static_cast<size_t>(y * glyph.bitmapWidth + x)];
				}
			}

			// 描画時にアトラス内の該当文字を切り出せるよう、ピクセル座標をUV座標へ変換する。
			glyph.glyphInfo.u0 = static_cast<float>(glyph.atlasX) / static_cast<float>(atlas.width);
			glyph.glyphInfo.v0 = static_cast<float>(glyph.atlasY) / static_cast<float>(atlas.height);
			glyph.glyphInfo.u1 = static_cast<float>(glyph.atlasX + glyph.bitmapWidth) / static_cast<float>(atlas.width);
			glyph.glyphInfo.v1 = static_cast<float>(glyph.atlasY + glyph.bitmapHeight) / static_cast<float>(atlas.height);

			rowCursorX += glyph.bitmapWidth + kPadding;
		}
	}

	atlas.glyphs = std::move(glyphs);
	atlas.isValid = true;
	return atlas;
}

void FontConverter::SaveAtlasAsPgm(const std::wstring& outputPath, const FontAtlas& atlas) const
{
	if (!atlas.isValid || atlas.width <= 0 || atlas.height <= 0 || atlas.pixels.empty())
	{
		std::wcerr << L"[FontConverter] Atlas is invalid. Skip atlas pgm output.\n";
		return;
	}

	std::ofstream ofs(std::filesystem::path(outputPath), std::ios::binary | std::ios::trunc);
	if (!ofs)
	{
		std::wcerr << L"[FontConverter] Failed to create atlas pgm file: " << outputPath << L"\n";
		return;
	}

	ofs << "P5\n";
	ofs << atlas.width << " " << atlas.height << "\n";
	ofs << "255\n";
	ofs.write(
		reinterpret_cast<const char*>(atlas.pixels.data()),
		static_cast<std::streamsize>(atlas.pixels.size())
	);
}

void FontConverter::SaveAlphaImageAsPng(
	const std::wstring& outputPath,
	int width,
	int height,
	const std::vector<std::uint8_t>& alphaPixels
) const
{
	if (width <= 0 || height <= 0)
	{
		std::wcerr << L"[FontConverter] Invalid image size for png: " << outputPath << L"\n";
		return;
	}

	if (alphaPixels.empty())
	{
		std::wcerr << L"[FontConverter] Empty pixel data for png: " << outputPath << L"\n";
		return;
	}

	if (alphaPixels.size() != static_cast<size_t>(width * height))
	{
		std::wcerr << L"[FontConverter] Pixel size mismatch for png: " << outputPath << L"\n";
		return;
	}

	using Microsoft::WRL::ComPtr;

	// WICを使って、アルファ画像をPNGとして保存する。
	ComPtr<IWICImagingFactory> factory;
	HRESULT hr = CoCreateInstance(
		CLSID_WICImagingFactory,
		nullptr,
		CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&factory)
	);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to create WIC factory.\n";
		return;
	}

	ComPtr<IWICStream> stream;
	hr = factory->CreateStream(&stream);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to create WIC stream.\n";
		return;
	}

	hr = stream->InitializeFromFilename(outputPath.c_str(), GENERIC_WRITE);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to initialize WIC stream: " << outputPath << L"\n";
		return;
	}

	ComPtr<IWICBitmapEncoder> encoder;
	hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to create PNG encoder.\n";
		return;
	}

	hr = encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to initialize PNG encoder.\n";
		return;
	}

	ComPtr<IWICBitmapFrameEncode> frame;
	ComPtr<IPropertyBag2> propertyBag;
	hr = encoder->CreateNewFrame(&frame, &propertyBag);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to create PNG frame.\n";
		return;
	}

	hr = frame->Initialize(propertyBag.Get());
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to initialize PNG frame.\n";
		return;
	}

	hr = frame->SetSize(static_cast<UINT>(width), static_cast<UINT>(height));
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to set PNG size.\n";
		return;
	}

	WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppRGBA;
	hr = frame->SetPixelFormat(&pixelFormat);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to set PNG pixel format.\n";
		return;
	}

	// アルファのみの画像を、白色RGBA画像へ変換してPNGに書き込む。
	std::vector<std::uint8_t> rgbaPixels(static_cast<size_t>(width * height * 4), 0);

	for (int i = 0; i < width * height; ++i)
	{
		const std::uint8_t a = alphaPixels[static_cast<size_t>(i)];

		rgbaPixels[static_cast<size_t>(i * 4 + 0)] = 255; // R
		rgbaPixels[static_cast<size_t>(i * 4 + 1)] = 255; // G
		rgbaPixels[static_cast<size_t>(i * 4 + 2)] = 255; // B
		rgbaPixels[static_cast<size_t>(i * 4 + 3)] = a;   // A
	}

	const UINT stride = static_cast<UINT>(width * 4);
	const UINT imageSize = static_cast<UINT>(rgbaPixels.size());

	hr = frame->WritePixels(
		static_cast<UINT>(height),
		stride,
		imageSize,
		reinterpret_cast<BYTE*>(rgbaPixels.data())
	);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to write PNG pixels.\n";
		return;
	}

	hr = frame->Commit();
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to commit PNG frame.\n";
		return;
	}

	hr = encoder->Commit();
	if (FAILED(hr))
	{
		std::wcerr << L"[FontConverter] Failed to commit PNG encoder.\n";
		return;
	}

	std::wcout << L"[FontConverter] Alpha PNG write success: " << outputPath << L"\n";
}