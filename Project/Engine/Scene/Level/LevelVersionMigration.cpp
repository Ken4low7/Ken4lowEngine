#include "LevelVersionMigration.h"

#include "LevelDocument.h"

namespace Ken4lowEngine
{
	bool LevelVersionMigration::MigrateToCurrent(nlohmann::json& levelJson, uint32_t& outSourceVersion, std::string& outError)
	{
		outError.clear();
		outSourceVersion = 0;

		if (!levelJson.is_object() || levelJson.value("Format", std::string{}) != "Ken4lowLevel")
		{
			outError = "Ken4lowLevel形式ではありません。";
			return false;
		}

		const int sourceVersion = levelJson.value("Version", 0);
		if (sourceVersion <= 0)
		{
			outError = "Level Versionが指定されていません。";
			return false;
		}
		if (sourceVersion > static_cast<int>(LevelDocument::kCurrentVersion))
		{
			outError = "現在のEngineより新しいLevel Versionです: " + std::to_string(sourceVersion);
			return false;
		}

		outSourceVersion = static_cast<uint32_t>(sourceVersion);
		int version = sourceVersion;
		while (version < static_cast<int>(LevelDocument::kCurrentVersion))
		{
			switch (version)
			{
			case 1:
				if (!MigrateVersion1To2(levelJson, outError)) return false;
				version = 2;
				break;
			case 2:
				if (!MigrateVersion2To3(levelJson, outError)) return false;
				version = 3;
				break;
			default:
				outError = "Migration経路が定義されていないLevel Versionです: " + std::to_string(version);
				return false;
			}
		}

		levelJson["Version"] = LevelDocument::kCurrentVersion;
		return true;
	}

	bool LevelVersionMigration::MigrateVersion1To2(nlohmann::json& levelJson, std::string& outError)
	{
		if (!levelJson.contains("Actors") || !levelJson["Actors"].is_array())
		{
			outError = "Version 1 LevelのActorsが不正です。";
			return false;
		}

		if (!levelJson.contains("LevelSettings") || !levelJson["LevelSettings"].is_object())
		{
			levelJson["LevelSettings"] = nlohmann::json::object();
		}
		if (!levelJson.contains("Environment") || !levelJson["Environment"].is_object())
		{
			levelJson["Environment"] = nlohmann::json::object();
		}

		for (nlohmann::json& actorEntry : levelJson["Actors"])
		{
			if (!actorEntry.is_object())
			{
				outError = "Version 1 Levelに不正なActorエントリがあります。";
				return false;
			}

			if (!actorEntry.contains("Editor") || !actorEntry["Editor"].is_object())
			{
				actorEntry["Editor"] = {
					{ "Visible", true },
					{ "Locked", false },
					{ "Folder", "" },
				};
			}

			if (actorEntry.contains("PrefabPath") && actorEntry["PrefabPath"].is_string())
			{
				nlohmann::json prefab = nlohmann::json::object();
				prefab["Path"] = actorEntry["PrefabPath"];
				prefab["Overrides"] = actorEntry.value("Overrides", nlohmann::json::object());
				actorEntry["Prefab"] = std::move(prefab);
				actorEntry.erase("PrefabPath");
				actorEntry.erase("Overrides");
			}
		}

		levelJson["Version"] = 2;
		return true;
	}

	bool LevelVersionMigration::MigrateVersion2To3(nlohmann::json& levelJson, std::string& outError)
	{
		if (!levelJson.contains("Actors") || !levelJson["Actors"].is_array())
		{
			outError = "Version 2 LevelのActorsが不正です。";
			return false;
		}

		if (!levelJson.contains("SubLevels") || !levelJson["SubLevels"].is_array())
		{
			levelJson["SubLevels"] = nlohmann::json::array();
		}
		if (!levelJson.contains("WorldPartition") || !levelJson["WorldPartition"].is_object())
		{
			levelJson["WorldPartition"] = {
				{ "Enabled", false },
				{ "CellSize", 128.0f },
				{ "LoadRadiusCells", 1 },
				{ "UnloadRadiusCells", 2 },
			};
		}

		levelJson["Version"] = 3;
		return true;
	}
} // namespace Ken4lowEngine
