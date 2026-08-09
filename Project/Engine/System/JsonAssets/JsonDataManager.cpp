#include "JsonDataManager.h"

#include <fstream>
#include <filesystem>
#include <iostream>

namespace Ken4lowEngine
{
	namespace
	{
		nlohmann::json ReadRootOrFallback(const nlohmann::json& root)
		{
			if (!root.is_object())
			{
				return nlohmann::json::object();
			}
			return root;
		}
	}

	bool JsonDataManager::SafeLoad(const std::string& filePath, JsonAssetEntry& outEntry)
	{
		if (!std::filesystem::exists(filePath))
		{
			// JSONが存在しない場合でもエディタを起動できるように安全読み込みを行う
			std::cout << "[JsonDataManager] not found: " << filePath << std::endl;
			return false;
		}

		try
		{
			std::ifstream ifs(filePath);
			if (!ifs)
			{
				std::cout << "[JsonDataManager] open failed: " << filePath << std::endl;
				return false;
			}

			nlohmann::json root;
			ifs >> root;
			root = ReadRootOrFallback(root);

			outEntry.version = root.value("version", 1);
			outEntry.id = root.value("id", "");
			outEntry.displayName = root.value("displayName", outEntry.id);
			outEntry.type = root.value("type", "UnknownType");
			outEntry.data = root.value("data", nlohmann::json::object());
			outEntry.path = filePath;
			outEntry.dirty = false;
			return true;
		}
		catch (const std::exception& e)
		{
			std::cout << "[JsonDataManager] parse failed: " << filePath << " error=" << e.what() << std::endl;
			return false;
		}
	}

	bool JsonDataManager::SafeSave(const JsonAssetEntry& entry)
	{
		try
		{
			std::filesystem::path path(entry.path);
			if (!path.parent_path().empty())
			{
				std::filesystem::create_directories(path.parent_path());
			}

			std::ofstream ofs(entry.path);
			if (!ofs)
			{
				std::cout << "[JsonDataManager] save failed: " << entry.path << std::endl;
				return false;
			}
			ofs << BuildRootJson(entry).dump(2);
			return true;
		}
		catch (const std::exception& e)
		{
			std::cout << "[JsonDataManager] exception on save: " << entry.path << " error=" << e.what() << std::endl;
			return false;
		}
	}

	bool JsonDataManager::Exists(const std::string& filePath)
	{
		return std::filesystem::exists(filePath);
	}

	bool JsonDataManager::Create(const JsonAssetEntry& entry)
	{
		if (Exists(entry.path))
		{
			return false;
		}
		return SafeSave(entry);
	}

	bool JsonDataManager::Delete(const std::string& filePath)
	{
		if (!Exists(filePath))
		{
			return false;
		}
		return std::filesystem::remove(filePath);
	}

	bool JsonDataManager::Duplicate(const JsonAssetEntry& source, const std::string& destinationPath, const std::string& newId, JsonAssetEntry& outDuplicated)
	{
		outDuplicated = source;
		outDuplicated.id = newId;
		outDuplicated.displayName = source.displayName + " Copy";
		outDuplicated.path = destinationPath;
		outDuplicated.dirty = false;
		return SafeSave(outDuplicated);
	}

	nlohmann::json JsonDataManager::BuildRootJson(const JsonAssetEntry& entry)
	{
		return {
			{ "version", entry.version },
			{ "id", entry.id },
			{ "displayName", entry.displayName },
			{ "type", entry.type },
			{ "data", entry.data }
		};
	}
}
