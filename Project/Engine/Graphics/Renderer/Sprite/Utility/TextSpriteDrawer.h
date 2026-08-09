#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Sprite.h"
#include "Vector2.h"
#include "Vector4.h"

namespace Ken4lowEngine
{

	class TextSpriteDrawer
	{
	private: /// ---------- 列挙型 ---------- ///

		enum class Align
		{
			Left,
			Center,
			Right
		};

	public: /// ---------- 構造体 ---------- ///

		struct GlyphInfo
		{
			Vector2 uvLeftTop{};   // atlas上の左上(px)
			Vector2 uvSize{};      // atlas上のサイズ(px)
			float advanceX = 0.0f; // 次文字へ進む量(px)

			float bearingX = 0.0f; // ベアリングX（グリフの左端から描画開始位置までの距離）
			float bearingY = 0.0f; // ベアリングY（ベースラインから描画開始位置までの距離）
		};

		struct FontDefinition
		{
			std::string texturePath;
			float baseDrawWidth = 0.0f;
			float baseDrawHeight = 0.0f;
			char32_t fallbackCodepoint = U'?';
			std::unordered_map<char32_t, GlyphInfo> glyphs;
		};

	public:

		void Initialize(const FontDefinition& fontDef);
		void Finalize();

		void Reset() { currentIndex_ = 0; }

		void SetColor(const Vector4& color) { color_ = color; }
		void SetScale(float scale) { scale_ = (scale > 0.0f) ? scale : 1.0f; }
		void SetLetterSpacing(float spacing) { letterSpacing_ = spacing; }
		void SetLineSpacing(float spacing) { lineSpacing_ = spacing; }
		void SetFallbackCodepoint(char32_t c) { fallbackCodepoint_ = c; }

		void RegisterGlyph(char32_t codepoint, const GlyphInfo& glyph);

		void DrawTextLeftAligned(const std::string& utf8Text, const Vector2& position);
		void DrawTextCentered(const std::string& utf8Text, const Vector2& centerPosition);
		void DrawTextRightAligned(const std::string& utf8Text, const Vector2& rightPosition);

		float MeasureWidth(const std::string& utf8Text) const;

	private:
		void DrawTextInternal(const std::u32string& text, const Vector2& position, Align align);
		float ComputeLineWidth(const std::u32string& line) const;

		const GlyphInfo* FindGlyph(char32_t codepoint) const;
		Sprite* AcquireSprite();

		static std::u32string Utf8ToUtf32(const std::string& utf8);
		static std::vector<std::u32string> SplitLines(const std::u32string& text);

	private:
		std::string texturePath_;

		float baseDrawW_ = 0.0f;
		float baseDrawH_ = 0.0f;
		float scale_ = 1.0f;

		float letterSpacing_ = 0.0f;
		float lineSpacing_ = 0.0f;
		float spaceAdvance_ = 16.0f; // スペース文字の進み量。glyphTable_にスペースがない場合の代替値として使う

		char32_t fallbackCodepoint_ = U'?';
		Vector4 color_{ 1.0f, 1.0f, 1.0f, 1.0f };

		std::unordered_map<char32_t, GlyphInfo> glyphTable_;
		mutable std::unordered_set<char32_t> warnedMissingGlyphs_;
		std::vector<std::unique_ptr<Sprite>> reusable_;
		size_t currentIndex_ = 0;
	};

} // namespace Ken4lowEngine
