#define NOMINMAX
#include "TextSpriteDrawer.h"
#include <TextureManager.h>
#include <algorithm>
#include <cassert>
#include <iomanip>
#include <sstream>

#include <Windows.h>

namespace Ken4lowEngine
{

	void TextSpriteDrawer::Initialize(const FontDefinition& fontDef)
	{
		Finalize();

		texturePath_ = fontDef.texturePath;
		baseDrawW_ = fontDef.baseDrawWidth;
		baseDrawH_ = fontDef.baseDrawHeight;
		fallbackCodepoint_ = fontDef.fallbackCodepoint;
		glyphTable_ = fontDef.glyphs;

		TextureManager::GetInstance()->LoadTexture(texturePath_);

		// サイズ未指定なら最初のグリフから仮決め
		if ((baseDrawW_ <= 0.0f || baseDrawH_ <= 0.0f) && !glyphTable_.empty())
		{
			const auto& firstGlyph = glyphTable_.begin()->second;
			if (baseDrawW_ <= 0.0f) { baseDrawW_ = firstGlyph.uvSize.x; }
			if (baseDrawH_ <= 0.0f) { baseDrawH_ = firstGlyph.uvSize.y; }
		}

		currentIndex_ = 0;

		auto itSpace = glyphTable_.find(U' ');
		if (itSpace != glyphTable_.end())
		{
			spaceAdvance_ = itSpace->second.advanceX;
		}
		else
		{
			// 空白グリフが無い場合は仮の進み幅を使う
			// 数字や英字の advance を使えればそれを流用
			auto itZero = glyphTable_.find(U'0');
			if (itZero != glyphTable_.end())
			{
				spaceAdvance_ = itZero->second.advanceX * 0.5f;
			}
			else
			{
				spaceAdvance_ = (baseDrawW_ > 0.0f) ? (baseDrawW_ * 0.5f) : 16.0f;
			}
		}
	}

	void TextSpriteDrawer::Finalize()
	{
		for (auto& sp : reusable_)
		{
			if (sp)
			{
				sp->Finalize();
			}
		}

		reusable_.clear();
		glyphTable_.clear();
		warnedMissingGlyphs_.clear();
		currentIndex_ = 0;

		texturePath_.clear();
		baseDrawW_ = 0.0f;
		baseDrawH_ = 0.0f;
		scale_ = 1.0f;
		letterSpacing_ = 0.0f;
		lineSpacing_ = 0.0f;
		fallbackCodepoint_ = U'?';
		color_ = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	void TextSpriteDrawer::RegisterGlyph(char32_t codepoint, const GlyphInfo& glyph)
	{
		glyphTable_[codepoint] = glyph;
	}

	const TextSpriteDrawer::GlyphInfo* TextSpriteDrawer::FindGlyph(char32_t codepoint) const
	{
		auto it = glyphTable_.find(codepoint);
		if (it != glyphTable_.end())
		{
			return &it->second;
		}

		auto fallbackIt = glyphTable_.find(fallbackCodepoint_);
		if (fallbackIt != glyphTable_.end())
		{
			if (warnedMissingGlyphs_.insert(codepoint).second)
			{
				std::ostringstream oss;
				oss << "[TextSpriteDrawer] Missing glyph: U+"
					<< std::uppercase << std::hex << std::setw(4) << std::setfill('0') << static_cast<uint32_t>(codepoint)
					<< " fallback=U+" << static_cast<uint32_t>(fallbackCodepoint_) << "\n";
				// 不足しているグリフを確認しやすくするため、同じcodepointは一度だけ警告する。
				OutputDebugStringA(oss.str().c_str());
			}
			return &fallbackIt->second;
		}

		if (warnedMissingGlyphs_.insert(codepoint).second)
		{
			std::ostringstream oss;
			oss << "[TextSpriteDrawer] Missing glyph: U+"
				<< std::uppercase << std::hex << std::setw(4) << std::setfill('0') << static_cast<uint32_t>(codepoint)
				<< " no fallback glyph\n";
			// 不足しているグリフを確認しやすくするため、同じcodepointは一度だけ警告する。
			OutputDebugStringA(oss.str().c_str());
		}
		return nullptr;
	}

	Sprite* TextSpriteDrawer::AcquireSprite()
	{
		if (currentIndex_ >= reusable_.size())
		{
			auto sp = std::make_unique<Sprite>();
			sp->Initialize(texturePath_);
			sp->SetAnchorPoint({ 0.0f, 0.0f });
			reusable_.push_back(std::move(sp));
		}

		return reusable_[currentIndex_++].get();
	}

	std::u32string TextSpriteDrawer::Utf8ToUtf32(const std::string& utf8)
	{
		std::u32string result;
		result.reserve(utf8.size());

		size_t i = 0;
		while (i < utf8.size())
		{
			const unsigned char c = static_cast<unsigned char>(utf8[i]);

			if (c <= 0x7F)
			{
				result.push_back(static_cast<char32_t>(c));
				++i;
			}
			else if ((c & 0xE0) == 0xC0)
			{
				if (i + 1 >= utf8.size()) { break; }
				char32_t cp =
					((c & 0x1F) << 6) |
					(static_cast<unsigned char>(utf8[i + 1]) & 0x3F);
				result.push_back(cp);
				i += 2;
			}
			else if ((c & 0xF0) == 0xE0)
			{
				if (i + 2 >= utf8.size()) { break; }
				char32_t cp =
					((c & 0x0F) << 12) |
					((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 6) |
					(static_cast<unsigned char>(utf8[i + 2]) & 0x3F);
				result.push_back(cp);
				i += 3;
			}
			else if ((c & 0xF8) == 0xF0)
			{
				if (i + 3 >= utf8.size()) { break; }
				char32_t cp =
					((c & 0x07) << 18) |
					((static_cast<unsigned char>(utf8[i + 1]) & 0x3F) << 12) |
					((static_cast<unsigned char>(utf8[i + 2]) & 0x3F) << 6) |
					(static_cast<unsigned char>(utf8[i + 3]) & 0x3F);
				result.push_back(cp);
				i += 4;
			}
			else
			{
				// 不正UTF-8は飛ばす
				++i;
			}
		}

		return result;
	}

	std::vector<std::u32string> TextSpriteDrawer::SplitLines(const std::u32string& text)
	{
		std::vector<std::u32string> lines;
		std::u32string current;

		for (char32_t c : text)
		{
			if (c == U'\r')
			{
				continue;
			}
			if (c == U'\n')
			{
				lines.push_back(current);
				current.clear();
				continue;
			}
			current.push_back(c);
		}

		lines.push_back(current);
		return lines;
	}

	float TextSpriteDrawer::ComputeLineWidth(const std::u32string& line) const
	{
		float width = 0.0f;
		bool first = true;

		for (char32_t c : line)
		{
			if (c == U' ')
			{
				if (!first)
				{
					width += letterSpacing_;
				}

				width += spaceAdvance_ * scale_;
				first = false;
				continue;
			}

			const GlyphInfo* glyph = FindGlyph(c);
			if (!glyph)
			{
				continue;
			}

			if (!first)
			{
				width += letterSpacing_;
			}

			width += glyph->advanceX * scale_;
			first = false;
		}

		return width;
	}

	float TextSpriteDrawer::MeasureWidth(const std::string& utf8Text) const
	{
		return ComputeLineWidth(Utf8ToUtf32(utf8Text));
	}

	void TextSpriteDrawer::DrawTextLeftAligned(const std::string& utf8Text, const Vector2& position)
	{
		DrawTextInternal(Utf8ToUtf32(utf8Text), position, Align::Left);
	}

	void TextSpriteDrawer::DrawTextCentered(const std::string& utf8Text, const Vector2& centerPosition)
	{
		DrawTextInternal(Utf8ToUtf32(utf8Text), centerPosition, Align::Center);
	}

	void TextSpriteDrawer::DrawTextRightAligned(const std::string& utf8Text, const Vector2& rightPosition)
	{
		DrawTextInternal(Utf8ToUtf32(utf8Text), rightPosition, Align::Right);
	}

	void TextSpriteDrawer::DrawTextInternal(const std::u32string& text, const Vector2& position, Align align)
	{
		const auto lines = SplitLines(text);

		float y = position.y;

		for (const auto& line : lines)
		{
			const float lineWidth = ComputeLineWidth(line);

			float startX = position.x;
			switch (align)
			{
			case Align::Left:
				startX = position.x;
				break;
			case Align::Center:
				startX = position.x - lineWidth * 0.5f;
				break;
			case Align::Right:
				startX = position.x - lineWidth;
				break;
			}

			float x = startX;
			bool first = true;

			for (char32_t c : line)
			{
				if (c == U' ')
				{
					if (!first)
					{
						x += letterSpacing_;
					}

					x += spaceAdvance_ * scale_;
					first = false;
					continue;
				}

				const GlyphInfo* glyph = FindGlyph(c);
				if (!glyph)
				{
					continue;
				}

				if (!first)
				{
					x += letterSpacing_;
				}

				Sprite* sp = AcquireSprite();
				sp->SetColor(color_);
				sp->SetUVRect(glyph->uvLeftTop, glyph->uvSize);
				sp->SetSize({ glyph->uvSize.x * scale_, glyph->uvSize.y * scale_ });

				const float drawX = x + glyph->bearingX * scale_;
				const float drawY = y + (baseDrawH_ - glyph->bearingY) * scale_; // ベースライン基準で縦位置を揃える
				sp->SetPosition({ drawX, drawY });
				
				sp->Update();
				sp->Draw();

				x += glyph->advanceX * scale_;
				first = false;
			}

			y += (baseDrawH_ * scale_) + lineSpacing_;
		}
	}

} // namespace Ken4lowEngine
