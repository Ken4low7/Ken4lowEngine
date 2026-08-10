#pragma once

#include <json.hpp>

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>Ken4lowLevelの旧Versionを現在Versionへ順番に変換する。</summary>
	class LevelVersionMigration
	{
	public:
		static bool MigrateToCurrent(nlohmann::json& levelJson, uint32_t& outSourceVersion, std::string& outError);

	private:
		static bool MigrateVersion1To2(nlohmann::json& levelJson, std::string& outError);
		static bool MigrateVersion2To3(nlohmann::json& levelJson, std::string& outError);
	};
} // namespace Ken4lowEngine
