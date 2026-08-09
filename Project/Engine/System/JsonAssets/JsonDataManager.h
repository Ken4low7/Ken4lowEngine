#pragma once

#include "JsonAssetEntry.h"

#include <optional>
#include <string>

namespace Ken4lowEngine
{
	class JsonDataManager
	{
	public:
		static bool SafeLoad(const std::string& filePath, JsonAssetEntry& outEntry);
		static bool SafeSave(const JsonAssetEntry& entry);
		static bool Exists(const std::string& filePath);
		static bool Create(const JsonAssetEntry& entry);
		static bool Delete(const std::string& filePath);
		static bool Duplicate(const JsonAssetEntry& source, const std::string& destinationPath, const std::string& newId, JsonAssetEntry& outDuplicated);

	private:
		static nlohmann::json BuildRootJson(const JsonAssetEntry& entry);
	};
}
