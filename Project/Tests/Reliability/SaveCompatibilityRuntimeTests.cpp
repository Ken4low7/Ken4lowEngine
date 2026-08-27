#include "SaveCompatibility.h"

#include <cassert>
#include <iostream>

using namespace Ken4lowEngine;

int main()
{
	nlohmann::json version1 = {
		{ "Format", "Ken4lowLevel" },
		{ "Version", 1 },
		{ "Actors", nlohmann::json::array() },
	};
	const SaveCompatibilityReport version1Report = SaveCompatibility::Inspect(version1);
	assert(version1Report.compatible);
	assert(version1Report.requiresMigration);
	assert(version1Report.sourceVersion == 1);
	assert(version1Report.targetVersion == LevelDocument::kCurrentVersion);

	nlohmann::json migrated;
	SaveCompatibilityReport migrationReport{};
	assert(SaveCompatibility::MigrateCopy(version1, migrated, migrationReport));
	assert(migrated["Version"].get<uint32_t>() == LevelDocument::kCurrentVersion);
	assert(migrated.contains("LevelSettings"));
	assert(migrated.contains("Environment"));
	assert(migrated.contains("WorldPartition"));
	assert(migrated.contains("SubLevels"));

	nlohmann::json version2 = {
		{ "Format", "Ken4lowLevel" },
		{ "Version", 2 },
		{ "Actors", nlohmann::json::array() },
	};
	const SaveCompatibilityReport version2Report = SaveCompatibility::Inspect(version2);
	assert(version2Report.compatible);
	assert(version2Report.requiresMigration);

	nlohmann::json current = {
		{ "Format", "Ken4lowLevel" },
		{ "Version", LevelDocument::kCurrentVersion },
		{ "Actors", nlohmann::json::array() },
	};
	const SaveCompatibilityReport currentReport = SaveCompatibility::Inspect(current);
	assert(currentReport.compatible);
	assert(!currentReport.requiresMigration);

	// Saves from a future schema are rejected rather than silently downgrading unknown data.
	nlohmann::json future = current;
	future["Version"] = LevelDocument::kCurrentVersion + 1;
	assert(!SaveCompatibility::Inspect(future).compatible);

	nlohmann::json invalid = current;
	invalid["Format"] = "OtherFormat";
	assert(!SaveCompatibility::Inspect(invalid).compatible);

	std::cout << "Save Compatibility runtime tests passed\n";
	return 0;
}
