#pragma once

#include "JsonAssetEntry.h"

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class JsonAssetRegistry
	{
	public:
		void Register(const JsonAssetEntry& entry);
		bool RemoveById(const std::string& id);
		JsonAssetEntry* FindById(const std::string& id);
		const std::vector<JsonAssetEntry>& GetAssets() const { return assets_; }
		std::vector<size_t> CollectFilteredIndices(const std::string& typeFilter) const;
		std::string MakeUniqueId(const std::string& baseId) const;

	private:
		std::vector<JsonAssetEntry> assets_;
	};
}
