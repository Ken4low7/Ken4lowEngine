#define NOMINMAX
#include "FontRasterizer.h"

#include "FontFileLoader.h"

#include <Windows.h>
#include <dwrite.h>
#include <iostream>
#include <vector>
#include <algorithm>

FontRasterizer::~FontRasterizer()
{
	// RegisterFontFileLoaderしたローダーは、終了時に必ず解除してDirectWrite側の参照を残さないようにする。
	if (dwriteFactory_ && fontFileLoader_)
	{
		dwriteFactory_->UnregisterFontFileLoader(fontFileLoader_);
	}

	if (fontFileLoader_)
	{
		fontFileLoader_->Release();
		fontFileLoader_ = nullptr;
	}
}

bool FontRasterizer::Initialize()
{
	if (dwriteFactory_)
	{
		// すでに初期化済みなら、重複してFactoryやLoaderを作らず成功扱いにする。
		return true;
	}

	// DirectWriteの入口となるFactoryを作成する。
	HRESULT hr = DWriteCreateFactory(
		DWRITE_FACTORY_TYPE_SHARED,
		__uuidof(IDWriteFactory),
		reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf())
	);

	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] Failed to create IDWriteFactory.\n";
		return false;
	}

	// 任意パスのフォントファイルを読み込むため、独自FontFileLoaderをDirectWriteへ登録する。
	fontFileLoader_ = new FontFileLoader();
	hr = dwriteFactory_->RegisterFontFileLoader(fontFileLoader_);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] Failed to register custom font file loader.\n";
		fontFileLoader_->Release();
		fontFileLoader_ = nullptr;
		return false;
	}

	std::wcout << L"[FontRasterizer] IDWriteFactory created successfully.\n";
	std::wcout << L"[FontRasterizer] Custom font file loader registered.\n";
	return true;
}

bool FontRasterizer::LoadFontFile(const std::wstring& filePath)
{
	if (filePath.empty())
	{
		std::wcerr << L"[FontRasterizer] Font file path is empty.\n";
		return false;
	}

	if (!dwriteFactory_ || !fontFileLoader_)
	{
		std::wcerr << L"[FontRasterizer] Factory or loader is not initialized.\n";
		return false;
	}

	// CreateCustomFontFileReferenceで参照できるよう、読み込むフォントパスをメンバに保持する。
	loadedFontPath_ = filePath;

	fontFile_.Reset();
	fontFace_.Reset();

	// 独自Loaderへ渡すキーとして、フォントファイルパス文字列をそのまま使用する。
	const void* fontKey = loadedFontPath_.c_str();
	const UINT32 fontKeySize = static_cast<UINT32>((loadedFontPath_.size() + 1) * sizeof(wchar_t));

	HRESULT hr = dwriteFactory_->CreateCustomFontFileReference(
		fontKey,
		fontKeySize,
		fontFileLoader_,
		&fontFile_
	);

	if (FAILED(hr) || !fontFile_)
	{
		std::wcerr << L"[FontRasterizer] Failed to create custom font file reference.\n";
		return false;
	}

	BOOL isSupported = FALSE;
	DWRITE_FONT_FILE_TYPE fileType = DWRITE_FONT_FILE_TYPE_UNKNOWN;
	DWRITE_FONT_FACE_TYPE faceType = DWRITE_FONT_FACE_TYPE_UNKNOWN;
	UINT32 numberOfFaces = 0;

	// フォント形式がDirectWriteで扱えるか、Face数が存在するかを調べる。
	hr = fontFile_->Analyze(&isSupported, &fileType, &faceType, &numberOfFaces);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] fontFile_->Analyze failed.\n";
		return false;
	}

	std::wcout << L"[FontRasterizer] Analyze succeeded. "
		<< L"supported=" << isSupported
		<< L" faces=" << numberOfFaces
		<< L" fileType=" << static_cast<int>(fileType)
		<< L" faceType=" << static_cast<int>(faceType)
		<< L"\n";

	if (!isSupported || numberOfFaces == 0)
	{
		std::wcerr << L"[FontRasterizer] Font file is not supported or has no faces.\n";
		return false;
	}

	// 解析済みのFontFileから、実際にグリフを取得するFontFaceを作成する。
	IDWriteFontFile* fontFiles[] = { fontFile_.Get() };
	hr = dwriteFactory_->CreateFontFace(
		faceType,
		1,
		fontFiles,
		0,
		DWRITE_FONT_SIMULATIONS_NONE,
		&fontFace_
	);

	if (FAILED(hr) || !fontFace_)
	{
		std::wcerr << L"[FontRasterizer] Failed to create IDWriteFontFace.\n";
		return false;
	}

	std::wcout << L"[FontRasterizer] Font path received: " << loadedFontPath_ << L"\n";
	std::wcout << L"[FontRasterizer] IDWriteFontFace created successfully.\n";

	return true;
}

bool FontRasterizer::HasGlyph(wchar_t character) const
{
	if (!fontFace_)
	{
		return false;
	}

	// wchar_tをUnicodeコードポイントとして扱い、フォント内のGlyphIndexへ変換する。
	const UINT32 codepoint = static_cast<UINT32>(character);
	UINT16 glyphIndex = 0;
	fontFace_->GetGlyphIndicesW(&codepoint, 1, &glyphIndex);

	// GlyphIndexが0の場合は、一般的にその文字がフォントに存在しない扱いにする。
	return glyphIndex != 0;
}

RasterizedGlyph FontRasterizer::RasterizeGlyph(wchar_t character, float fontSize) const
{
	if (!dwriteFactory_)
	{
		std::wcerr << L"[FontRasterizer] Factory is not initialized.\n";
		return RasterizedGlyph{};
	}

	if (loadedFontPath_.empty())
	{
		std::wcerr << L"[FontRasterizer] Font file is not loaded.\n";
		return RasterizedGlyph{};
	}

	if (!fontFace_)
	{
		std::wcerr << L"[FontRasterizer] FontFace is not available.\n";
		return RasterizedGlyph{};
	}

	RasterizedGlyph glyph{};

	// 文字をGlyphIndexへ変換し、以降のメトリクス取得とビットマップ化に使う。
	const UINT32 codepoint = static_cast<UINT32>(character);
	UINT16 glyphIndex = 0;
	fontFace_->GetGlyphIndicesW(&codepoint, 1, &glyphIndex);

	if (glyphIndex == 0)
	{
		std::wcerr << L"[FontRasterizer] Missing glyph: U+"
			<< std::hex << codepoint << std::dec
			<< L" ('" << character << L"').\n";
		return RasterizedGlyph{};
	}

	// フォント全体の基準値と、対象グリフの個別メトリクスを取得する。
	DWRITE_FONT_METRICS fontMetrics{};
	fontFace_->GetMetrics(&fontMetrics);

	DWRITE_GLYPH_METRICS glyphMetrics{};
	HRESULT hr = fontFace_->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE);
	if (FAILED(hr))
	{
		std::wcerr << L"[FontRasterizer] Failed to get glyph metrics: U+"
			<< std::hex << codepoint << std::dec
			<< L" ('" << character << L"').\n";
		return RasterizedGlyph{};
	}

	// DirectWriteのデザイン単位を、実際に出力するピクセルサイズへ変換する倍率。
	const float scale = fontSize / static_cast<float>(fontMetrics.designUnitsPerEm);

	const float width =
		static_cast<float>(glyphMetrics.advanceWidth - glyphMetrics.leftSideBearing - glyphMetrics.rightSideBearing) * scale;

	const float height =
		static_cast<float>(glyphMetrics.advanceHeight - glyphMetrics.topSideBearing - glyphMetrics.bottomSideBearing) * scale;

	// TextSpriteDrawerで参照するため、文字のサイズ・送り幅・ベアリング情報を保存する。
	glyph.glyphInfo.codepoint = codepoint;
	glyph.glyphInfo.u0 = 0.0f;
	glyph.glyphInfo.v0 = 0.0f;
	glyph.glyphInfo.u1 = 1.0f;
	glyph.glyphInfo.v1 = 1.0f;

	glyph.glyphInfo.width = (width > 0.0f) ? width : 1.0f;
	glyph.glyphInfo.height = (height > 0.0f) ? height : fontSize;
	glyph.glyphInfo.bearingX = static_cast<float>(glyphMetrics.leftSideBearing) * scale;
	glyph.glyphInfo.bearingY = static_cast<float>(glyphMetrics.verticalOriginY - glyphMetrics.topSideBearing) * scale;
	glyph.glyphInfo.advanceX = static_cast<float>(glyphMetrics.advanceWidth) * scale;

	glyph.bitmapWidth = std::max(1, static_cast<int>(glyph.glyphInfo.width));
	glyph.bitmapHeight = std::max(1, static_cast<int>(glyph.glyphInfo.height));

	// GlyphRunAnalysisを使って、実際のアルファビットマップを生成する。
	if (!BuildGlyphBitmap(glyphIndex, fontSize, glyphMetrics, fontMetrics, glyph))
	{
		std::wcerr << L"[FontRasterizer] Failed to build glyph bitmap: U+"
			<< std::hex << codepoint << std::dec
			<< L" ('" << character << L"').\n";
		return RasterizedGlyph{};
	}

	glyph.isValid = true;

	std::wcout << L"[FontRasterizer] Rasterized glyph from font: U+"
		<< std::hex << codepoint << std::dec
		<< L" ('" << character << L"')"
		<< L" size=" << fontSize
		<< L" bitmap=" << glyph.bitmapWidth << L"x" << glyph.bitmapHeight << L"\n";

	return glyph;
}

bool FontRasterizer::BuildGlyphBitmap(UINT16 glyphIndex, float fontSize, const DWRITE_GLYPH_METRICS& glyphMetrics, const DWRITE_FONT_METRICS& fontMetrics, RasterizedGlyph& outGlyph) const
{
	if (!dwriteFactory_ || !fontFace_)
	{
		return false;
	}

	// DirectWriteへ渡すemサイズ。ここでは出力したいフォントサイズをそのまま使う。
	const float emSize = fontSize;

	UINT16 glyphIndices[1] = { glyphIndex };
	FLOAT glyphAdvances[1] = { 0.0f };
	DWRITE_GLYPH_OFFSET glyphOffsets[1] = {};

	// 1文字分だけを描画対象にしたGlyphRunを構築する。
	DWRITE_GLYPH_RUN glyphRun{};
	glyphRun.fontFace = fontFace_.Get();
	glyphRun.fontEmSize = emSize;
	glyphRun.glyphCount = 1;
	glyphRun.glyphIndices = glyphIndices;
	glyphRun.glyphAdvances = glyphAdvances;
	glyphRun.glyphOffsets = glyphOffsets;
	glyphRun.isSideways = FALSE;
	glyphRun.bidiLevel = 0;

	Microsoft::WRL::ComPtr<IDWriteGlyphRunAnalysis> analysis;

	// GlyphRunAnalysisを作成し、文字の描画範囲とアルファテクスチャを取得できる状態にする。
	HRESULT hr = dwriteFactory_->CreateGlyphRunAnalysis(
		&glyphRun,
		1.0f,
		nullptr,
		DWRITE_RENDERING_MODE_ALIASED,
		DWRITE_MEASURING_MODE_NATURAL,
		0.0f,
		0.0f,
		&analysis
	);

	if (FAILED(hr) || !analysis)
	{
		return false;
	}

	// 実際に描画されるアルファテクスチャの矩形範囲を取得する。
	RECT textureBounds{};
	hr = analysis->GetAlphaTextureBounds(
		DWRITE_TEXTURE_ALIASED_1x1,
		&textureBounds
	);

	if (FAILED(hr))
	{
		return false;
	}

	const int width = textureBounds.right - textureBounds.left;
	const int height = textureBounds.bottom - textureBounds.top;

	if (width <= 0 || height <= 0)
	{
		// 空白など描画ピクセルがない文字でも、後続処理が壊れないよう1pxの透明画像として扱う。
		outGlyph.bitmapWidth = 1;
		outGlyph.bitmapHeight = 1;
		outGlyph.pixels.assign(1, 0);
		return true;
	}

	// DirectWriteから取得したアルファ値を、0〜255のグレースケールとして保持する。
	std::vector<std::uint8_t> alphaPixels(static_cast<size_t>(width * height), 0);

	hr = analysis->CreateAlphaTexture(
		DWRITE_TEXTURE_ALIASED_1x1,
		&textureBounds,
		alphaPixels.data(),
		static_cast<UINT32>(alphaPixels.size())
	);

	if (FAILED(hr))
	{
		return false;
	}

	outGlyph.bitmapWidth = width;
	outGlyph.bitmapHeight = height;
	outGlyph.pixels = std::move(alphaPixels);

	(void)glyphMetrics;
	(void)fontMetrics;

	return true;
}

RasterizedGlyph FontRasterizer::BuildFallbackGlyph(wchar_t character, float fontSize) const
{
	RasterizedGlyph glyph{};

	// フォントに存在しない文字を確認しやすいよう、仮の矩形グリフを作成する。
	glyph.glyphInfo.codepoint = static_cast<std::uint32_t>(character);
	glyph.glyphInfo.u0 = 0.0f;
	glyph.glyphInfo.v0 = 0.0f;
	glyph.glyphInfo.u1 = 1.0f;
	glyph.glyphInfo.v1 = 1.0f;

	glyph.glyphInfo.width = fontSize * 0.5f;
	glyph.glyphInfo.height = fontSize;
	glyph.glyphInfo.bearingX = 0.0f;
	glyph.glyphInfo.bearingY = fontSize;
	glyph.glyphInfo.advanceX = fontSize * 0.6f;

	glyph.bitmapWidth = static_cast<int>(fontSize * 0.5f);
	glyph.bitmapHeight = static_cast<int>(fontSize);

	if (glyph.bitmapWidth < 1)  glyph.bitmapWidth = 1;
	if (glyph.bitmapHeight < 1) glyph.bitmapHeight = 1;

	glyph.pixels.resize(static_cast<size_t>(glyph.bitmapWidth * glyph.bitmapHeight), 255);
	glyph.isValid = true;

	std::wcout << L"[FontRasterizer] Rasterized fallback glyph bitmap: U+"
		<< std::hex << static_cast<std::uint32_t>(character) << std::dec
		<< L" ('" << character << L"')"
		<< L" size=" << fontSize << L"\n";

	return glyph;
}