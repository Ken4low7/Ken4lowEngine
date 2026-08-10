#pragma once

#include <json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Level内Actorが参照するPrefabと、そのPrefabに対する差分Override。</summary>
	struct LevelPrefabReference
	{
		std::string path;
		nlohmann::json overrides = nlohmann::json::object();

		[[nodiscard]] bool IsSet() const { return !path.empty(); }
	};

	/// <summary>Level JSON内のActor 1件を表すDocument。</summary>
	struct LevelActorDocument
	{
		std::string id;
		std::string parentId;
		bool editorVisible = true;
		bool editorLocked = false;
		std::string editorFolder;

		// Prefabを使わないActorはdataをそのまま保存する。
		nlohmann::json data = nlohmann::json::object();
		LevelPrefabReference prefab{};

		// Load時にPrefab + Overrideを解決したActor JSON。ファイルへは直接保存しない。
		nlohmann::json resolvedData = nlohmann::json::object();
	};

	/// <summary>
	/// Editor / Runtimeの両方で共有するKen4lowLevelの正規インメモリ表現。
	/// JSONの読み書き・Migration・World生成を分離するための中間Documentとして使用する。
	/// </summary>
	struct LevelDocument
	{
		static constexpr uint32_t kCurrentVersion = 2;

		uint32_t sourceVersion = kCurrentVersion;
		bool migrated = false;
		std::string name = "Untitled";
		std::string targetScene;
		std::vector<LevelActorDocument> actors;
		nlohmann::json lighting = nlohmann::json::object();
		nlohmann::json camera = nlohmann::json::object();
		nlohmann::json environment = nlohmann::json::object();
	};
} // namespace Ken4lowEngine
