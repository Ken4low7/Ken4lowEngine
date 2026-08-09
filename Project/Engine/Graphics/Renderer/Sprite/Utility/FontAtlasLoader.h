#pragma once
#include <string>
#include "TextSpriteDrawer.h"

namespace Ken4lowEngine
{

	class FontAtlasLoader
	{
	public:
		// JSON から読む
		static TextSpriteDrawer::FontDefinition LoadFromJson(
			const std::string& texturePath,
			const std::string& jsonPath,
			float baseDrawWidth = 0.0f,
			float baseDrawHeight = 0.0f,
			char32_t fallbackCodepoint = U'?');

		// atlas.txt から読む
		static TextSpriteDrawer::FontDefinition LoadFromAtlasText(
			const std::string& texturePath,
			const std::string& atlasPath,
			float baseDrawWidth = 0.0f,
			float baseDrawHeight = 0.0f,
			char32_t fallbackCodepoint = U'?');

	private:
		static char32_t DecodeCodepoint(const std::string& s);
		static std::string Trim(const std::string& s);
		static bool TryParseKeyValue(const std::string& token, std::string& outKey, std::string& outValue);
	};

} // namespace Ken4lowEngine