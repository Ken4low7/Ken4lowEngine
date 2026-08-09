#pragma once

#include "CollisionPreset.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

/// CollisionPresetLibrary はコード既定値とJson定義を共存させる読み書き用の小さな入口。
class CollisionPresetLibrary
{
public:
	static constexpr const char* kDefaultJsonPath = "Resources/DataAssets/CollisionPresets/collision_presets.json";

	void ResetToDefaults();
	bool LoadFromJsonFile(std::string_view filePath = kDefaultJsonPath);
	bool SaveToJsonFile(std::string_view filePath = kDefaultJsonPath) const;

	const std::vector<CollisionPreset>& GetPresets() const { return presets_; }
	const CollisionPreset* FindPreset(std::string_view name) const;
	const CollisionPreset* GetPreset(size_t index) const;

	bool WasLoadedFromJson() const { return loadedFromJson_; }
	const std::string& GetLastStatus() const { return lastStatus_; }

private:
	std::vector<CollisionPreset> presets_ = GetDefaultCollisionPresets();
	std::string lastStatus_ = "Using built-in CollisionPreset defaults.";
	bool loadedFromJson_ = false;
};
