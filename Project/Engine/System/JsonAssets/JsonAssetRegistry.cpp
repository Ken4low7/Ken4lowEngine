#include "JsonAssetRegistry.h"

#include <algorithm>

namespace Ken4lowEngine
{
	void JsonAssetRegistry::Register(const JsonAssetEntry& entry)
	{
		if (JsonAssetEntry* existing = FindById(entry.id))
		{
			*existing = entry;
			return;
		}
		assets_.push_back(entry);
	}

	bool JsonAssetRegistry::RemoveById(const std::string& id)
	{
		auto it = std::remove_if(assets_.begin(), assets_.end(), [&](const JsonAssetEntry& e) { return e.id == id; });
		if (it == assets_.end())
		{
			return false;
		}
		assets_.erase(it, assets_.end());
		return true;
	}

	JsonAssetEntry* JsonAssetRegistry::FindById(const std::string& id)
	{
		for (auto& asset : assets_)
		{
			if (asset.id == id)
			{
				return &asset;
			}
		}
		return nullptr;
	}

	std::vector<size_t> JsonAssetRegistry::CollectFilteredIndices(const std::string& typeFilter) const
	{
		std::vector<size_t> indices;
		for (size_t i = 0; i < assets_.size(); ++i)
		{
			if (typeFilter.empty() || typeFilter == "All" || assets_[i].type == typeFilter)
			{
				indices.push_back(i);
			}
		}
		return indices;
	}

	std::string JsonAssetRegistry::MakeUniqueId(const std::string& baseId) const
	{
		std::string candidate = baseId;
		int suffix = 1;
		while (std::any_of(assets_.begin(), assets_.end(), [&](const JsonAssetEntry& e) { return e.id == candidate; }))
		{
			candidate = baseId + "_" + std::to_string(suffix++);
		}
		return candidate;
	}
}
