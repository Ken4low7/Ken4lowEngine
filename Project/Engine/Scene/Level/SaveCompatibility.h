#pragma once

#include "LevelDocument.h"
#include "LevelVersionMigration.h"

#include <json.hpp>

#include <cstdint>
#include <string>

namespace Ken4lowEngine
{
	struct SaveCompatibilityReport
	{
		bool compatible = false;
		bool requiresMigration = false;
		uint32_t sourceVersion = 0;
		uint32_t targetVersion = LevelDocument::kCurrentVersion;
		std::string message;
	};

	class SaveCompatibility
	{
	public:
		[[nodiscard]] static SaveCompatibilityReport Inspect(const nlohmann::json& source)
		{
			SaveCompatibilityReport report{};
			if (!source.is_object() || source.value("Format", std::string{}) != "Ken4lowLevel")
			{
				report.message = "Save format is not Ken4lowLevel.";
				return report;
			}

			const int version = source.value("Version", 0);
			if (version <= 0)
			{
				report.message = "Save version is missing or invalid.";
				return report;
			}
			report.sourceVersion = static_cast<uint32_t>(version);
			if (report.sourceVersion > LevelDocument::kCurrentVersion)
			{
				report.message = "Save was created by a newer engine version.";
				return report;
			}

			// Compatibility is defined by the same migration path used by LevelSerializer, never by a duplicate version table.
			nlohmann::json migrated = source;
			uint32_t migratedFromVersion = 0;
			std::string migrationError;
			if (!LevelVersionMigration::MigrateToCurrent(migrated, migratedFromVersion, migrationError))
			{
				report.message = migrationError;
				return report;
			}

			report.compatible = true;
			report.requiresMigration = report.sourceVersion != LevelDocument::kCurrentVersion;
			report.message = report.requiresMigration ? "Save can be migrated to the current version." : "Save is current.";
			return report;
		}

		static bool MigrateCopy(const nlohmann::json& source, nlohmann::json& outMigrated, SaveCompatibilityReport& outReport)
		{
			outReport = Inspect(source);
			if (!outReport.compatible)
			{
				return false;
			}

			outMigrated = source;
			uint32_t sourceVersion = 0;
			std::string error;
			if (!LevelVersionMigration::MigrateToCurrent(outMigrated, sourceVersion, error))
			{
				outReport.compatible = false;
				outReport.message = error;
				return false;
			}
			return true;
		}
	};
} // namespace Ken4lowEngine
