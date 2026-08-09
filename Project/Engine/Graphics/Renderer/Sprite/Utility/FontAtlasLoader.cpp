#define NOMINMAX
#include "FontAtlasLoader.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cctype>

#include <json.hpp>

namespace Ken4lowEngine
{
	namespace
	{
		using json = nlohmann::json;

		std::vector<std::string> SplitBySpaceRespectQuote(const std::string& line)
		{
			std::vector<std::string> result;
			std::string current;
			bool inQuote = false;

			for (char c : line)
			{
				if (c == '"')
				{
					inQuote = !inQuote;
					current.push_back(c);
					continue;
				}

				if (!inQuote && std::isspace(static_cast<unsigned char>(c)))
				{
					if (!current.empty())
					{
						result.push_back(current);
						current.clear();
					}
					continue;
				}

				current.push_back(c);
			}

			if (!current.empty())
			{
				result.push_back(current);
			}

			return result;
		}
	}

	std::string FontAtlasLoader::Trim(const std::string& s)
	{
		const auto begin = std::find_if_not(s.begin(), s.end(),
			[](unsigned char c) { return std::isspace(c) != 0; });

		if (begin == s.end())
		{
			return {};
		}

		const auto rbegin = std::find_if_not(s.rbegin(), s.rend(),
			[](unsigned char c) { return std::isspace(c) != 0; });

		return std::string(begin, rbegin.base());
	}

	bool FontAtlasLoader::TryParseKeyValue(const std::string& token, std::string& outKey, std::string& outValue)
	{
		const size_t eq = token.find('=');
		if (eq == std::string::npos)
		{
			return false;
		}

		outKey = token.substr(0, eq);
		outValue = token.substr(eq + 1);

		// 前後の " を剥がす
		if (!outValue.empty() && outValue.front() == '"')
		{
			outValue.erase(outValue.begin());
		}
		if (!outValue.empty() && outValue.back() == '"')
		{
			outValue.pop_back();
		}

		return true;
	}

	char32_t FontAtlasLoader::DecodeCodepoint(const std::string& s)
	{
		if (s.empty())
		{
			return U'\0';
		}

		auto IsAllDigits = [](const std::string& str) -> bool
			{
				return !str.empty() &&
					std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
			};

		auto IsAllHexDigits = [](const std::string& str) -> bool
			{
				return !str.empty() &&
					std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
			};

		// 0x3042 / 0X3042
		if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0)
		{
			return static_cast<char32_t>(std::stoul(s, nullptr, 16));
		}

		// U+3042 / u+3042
		if (s.rfind("U+", 0) == 0 || s.rfind("u+", 0) == 0)
		{
			return static_cast<char32_t>(std::stoul(s.substr(2), nullptr, 16));
		}

		// プレーン16進コードを許容
		// 例: "0048" -> U+0048 ('H')
		//     "3042" -> U+3042 ('あ')
		//
		// ただし "65" のような短い数字は codepoint 10進表記の可能性もあるので、
		// 4桁以上の16進文字列だけを「16進コード」とみなす。
		if (s.size() >= 4 && IsAllHexDigits(s))
		{
			return static_cast<char32_t>(std::stoul(s, nullptr, 16));
		}

		// 数字だけなら codepoint の10進表記として解釈
		if (IsAllDigits(s))
		{
			return static_cast<char32_t>(std::stoul(s, nullptr, 10));
		}

		// UTF-8 の1文字そのものが来た場合の簡易対応
		const unsigned char c0 = static_cast<unsigned char>(s[0]);
		if (c0 <= 0x7F)
		{
			return static_cast<char32_t>(c0);
		}
		else if ((c0 & 0xE0) == 0xC0 && s.size() >= 2)
		{
			return ((c0 & 0x1F) << 6) |
				(static_cast<unsigned char>(s[1]) & 0x3F);
		}
		else if ((c0 & 0xF0) == 0xE0 && s.size() >= 3)
		{
			return ((c0 & 0x0F) << 12) |
				((static_cast<unsigned char>(s[1]) & 0x3F) << 6) |
				(static_cast<unsigned char>(s[2]) & 0x3F);
		}
		else if ((c0 & 0xF8) == 0xF0 && s.size() >= 4)
		{
			return ((c0 & 0x07) << 18) |
				((static_cast<unsigned char>(s[1]) & 0x3F) << 12) |
				((static_cast<unsigned char>(s[2]) & 0x3F) << 6) |
				(static_cast<unsigned char>(s[3]) & 0x3F);
		}

		return U'\0';
	}

	TextSpriteDrawer::FontDefinition FontAtlasLoader::LoadFromJson(const std::string& texturePath, const std::string& jsonPath, float baseDrawWidth, float baseDrawHeight, char32_t fallbackCodepoint)
	{
		TextSpriteDrawer::FontDefinition def{};
		def.texturePath = texturePath;
		def.baseDrawWidth = baseDrawWidth;
		def.baseDrawHeight = baseDrawHeight;
		def.fallbackCodepoint = fallbackCodepoint;

		std::ifstream ifs(jsonPath);
		if (!ifs.is_open())
		{
			throw std::runtime_error("FontAtlasLoader::LoadFromJson failed: cannot open file: " + jsonPath);
		}

		json root;
		ifs >> root;

		json glyphContainer;
		if (root.contains("glyphs"))
		{
			glyphContainer = root["glyphs"];
		}
		else if (root.contains("characters"))
		{
			glyphContainer = root["characters"];
		}
		else
		{
			throw std::runtime_error("FontAtlasLoader::LoadFromJson failed: glyph container not found in " + jsonPath);
		}

		// フォント生成側の標準サイズ情報があれば拾う
		if (def.baseDrawWidth <= 0.0f)
		{
			if (root.contains("cellWidth"))
			{
				def.baseDrawWidth = root["cellWidth"].get<float>();
			}
			else if (root.contains("fontSize"))
			{
				def.baseDrawWidth = root["fontSize"].get<float>();
			}
		}

		if (def.baseDrawHeight <= 0.0f)
		{
			if (root.contains("cellHeight"))
			{
				def.baseDrawHeight = root["cellHeight"].get<float>();
			}
			else if (root.contains("fontSize"))
			{
				def.baseDrawHeight = root["fontSize"].get<float>();
			}
		}

		if (glyphContainer.is_array())
		{
			for (const auto& item : glyphContainer)
			{
				char32_t codepoint = U'\0';

				if (item.contains("codepoint"))
				{
					codepoint = static_cast<char32_t>(item["codepoint"].get<uint32_t>());
				}
				else if (item.contains("char"))
				{
					codepoint = DecodeCodepoint(item["char"].get<std::string>());
				}
				else if (item.contains("glyph"))
				{
					codepoint = DecodeCodepoint(item["glyph"].get<std::string>());
				}

				if (codepoint == U'\0')
				{
					continue;
				}

				TextSpriteDrawer::GlyphInfo glyph{};

				// 位置: x/y または atlasX/atlasY
				glyph.uvLeftTop.x = item.value("x", item.value("atlasX", 0.0f));
				glyph.uvLeftTop.y = item.value("y", item.value("atlasY", 0.0f));

				// サイズ: w/h または width/height
				glyph.uvSize.x = item.value("w", item.value("width", 0.0f));
				glyph.uvSize.y = item.value("h", item.value("height", 0.0f));

				// 進み量: advance or advanceX
				glyph.advanceX = item.value("advance", item.value("advanceX", glyph.uvSize.x));

				glyph.bearingX = item.value("bearingX", 0.0f);
				glyph.bearingY = item.value("bearingY", glyph.uvSize.y);

				def.glyphs[codepoint] = glyph;
			}
		}
		else if (glyphContainer.is_object())
		{
			for (auto it = glyphContainer.begin(); it != glyphContainer.end(); ++it)
			{
				char32_t codepoint = DecodeCodepoint(it.key());
				if (codepoint == U'\0')
				{
					continue;
				}

				const auto& item = it.value();

				TextSpriteDrawer::GlyphInfo glyph{};
				glyph.uvLeftTop.x = item.value("x", item.value("atlasX", 0.0f));
				glyph.uvLeftTop.y = item.value("y", item.value("atlasY", 0.0f));
				glyph.uvSize.x = item.value("w", item.value("width", 0.0f));
				glyph.uvSize.y = item.value("h", item.value("height", 0.0f));
				glyph.advanceX = item.value("advance", item.value("advanceX", glyph.uvSize.x));

				def.glyphs[codepoint] = glyph;
			}
		}

		if ((!def.glyphs.empty()) && (def.baseDrawWidth <= 0.0f || def.baseDrawHeight <= 0.0f))
		{
			const auto& g = def.glyphs.begin()->second;
			if (def.baseDrawWidth <= 0.0f) { def.baseDrawWidth = g.uvSize.x; }
			if (def.baseDrawHeight <= 0.0f) { def.baseDrawHeight = g.uvSize.y; }
		}

		return def;
	}

	TextSpriteDrawer::FontDefinition FontAtlasLoader::LoadFromAtlasText(
		const std::string& texturePath,
		const std::string& atlasPath,
		float baseDrawWidth,
		float baseDrawHeight,
		char32_t fallbackCodepoint)
	{
		TextSpriteDrawer::FontDefinition def{};
		def.texturePath = texturePath;
		def.baseDrawWidth = baseDrawWidth;
		def.baseDrawHeight = baseDrawHeight;
		def.fallbackCodepoint = fallbackCodepoint;

		std::ifstream ifs(atlasPath);
		if (!ifs.is_open())
		{
			throw std::runtime_error("FontAtlasLoader::LoadFromAtlasText failed: cannot open file: " + atlasPath);
		}

		std::string line;
		while (std::getline(ifs, line))
		{
			line = Trim(line);
			if (line.empty())
			{
				continue;
			}

			// コメントスキップ
			if (line[0] == '#')
			{
				continue;
			}

			// BMFont風:
			// char id=65 x=0 y=0 width=32 height=32 xadvance=32
			// または
			// glyph char="A" x=0 y=0 w=32 h=32 advance=32
			const std::vector<std::string> tokens = SplitBySpaceRespectQuote(line);
			if (tokens.empty())
			{
				continue;
			}

			std::unordered_map<std::string, std::string> kv;
			for (size_t i = 1; i < tokens.size(); ++i)
			{
				std::string key;
				std::string value;
				if (TryParseKeyValue(tokens[i], key, value))
				{
					kv[key] = value;
				}
			}

			const std::string& head = tokens[0];

			// 共通サイズのヒント
			if (head == "common")
			{
				if (def.baseDrawWidth <= 0.0f && kv.contains("baseW"))
				{
					def.baseDrawWidth = std::stof(kv["baseW"]);
				}
				if (def.baseDrawHeight <= 0.0f && kv.contains("baseH"))
				{
					def.baseDrawHeight = std::stof(kv["baseH"]);
				}
				continue;
			}

			if (head != "char" && head != "glyph")
			{
				continue;
			}

			char32_t codepoint = U'\0';

			if (kv.contains("id"))
			{
				// id が "65" / "0041" / "0x0041" / "U+0041" のどれでも読めるようにする。
				codepoint = DecodeCodepoint(kv["id"]);
			}
			else if (kv.contains("char"))
			{
				codepoint = DecodeCodepoint(kv["char"]);
			}
			else if (kv.contains("codepoint"))
			{
				// codepoint も10進数固定ではなく、フォント生成形式に合わせて柔軟に読む。
				codepoint = DecodeCodepoint(kv["codepoint"]);
			}

			if (codepoint == U'\0')
			{
				continue;
			}

			TextSpriteDrawer::GlyphInfo glyph{};
			glyph.uvLeftTop.x = kv.contains("x") ? std::stof(kv["x"]) : 0.0f;
			glyph.uvLeftTop.y = kv.contains("y") ? std::stof(kv["y"]) : 0.0f;
			glyph.uvSize.x = kv.contains("w") ? std::stof(kv["w"]) :
				(kv.contains("width") ? std::stof(kv["width"]) : 0.0f);
			glyph.uvSize.y = kv.contains("h") ? std::stof(kv["h"]) :
				(kv.contains("height") ? std::stof(kv["height"]) : 0.0f);
			glyph.advanceX = kv.contains("advance") ? std::stof(kv["advance"]) :
				(kv.contains("xadvance") ? std::stof(kv["xadvance"]) : glyph.uvSize.x);

			def.glyphs[codepoint] = glyph;
		}

		if ((!def.glyphs.empty()) && (def.baseDrawWidth <= 0.0f || def.baseDrawHeight <= 0.0f))
		{
			const auto& g = def.glyphs.begin()->second;
			if (def.baseDrawWidth <= 0.0f) { def.baseDrawWidth = g.uvSize.x; }
			if (def.baseDrawHeight <= 0.0f) { def.baseDrawHeight = g.uvSize.y; }
		}

		return def;
	}

} // namespace Ken4lowEngine
