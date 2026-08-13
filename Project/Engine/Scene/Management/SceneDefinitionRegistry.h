#pragma once

#include "SceneDefinition.h"
#include "SceneDefinitionSerializer.h"
#include <Engine/Core/Project/ProjectSettings.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Scene Registry、Scene JSON、通常Levelを読み込み、Scene IDから定義を解決します。</summary>
	class SceneDefinitionRegistry
	{
	public:
		SceneDefinitionRegistry()
		{
			ProjectSettings* projectSettings = ProjectSettings::GetInstance();
			const std::filesystem::path registryPath = projectSettings->EnsureLoaded()
				? projectSettings->GetSceneRegistryPath()
				: "Resources/JSON/Scenes/SceneRegistry.json";
			Load(registryPath); // Project Settingsが壊れていても従来の標準RegistryへFallbackする。
		}

		bool Load(const std::filesystem::path& registryPath)
		{
			Clear();
			registryPath_ = registryPath;

			try
			{
				std::ifstream registryFile(registryPath);
				if (!registryFile.is_open())
				{
					lastError_ = "Scene Registryを開けません: " + registryPath.generic_string();
					RefreshDiscoveredLevelScenes();
					RegisterFallbackDefinitions();
					return false;
				}

				nlohmann::json registryJson;
				registryFile >> registryJson;
				if (!registryJson.is_object() || registryJson.value("Format", std::string{}) != "Ken4lowSceneRegistry")
				{
					lastError_ = "Scene Registry形式が不正です: " + registryPath.generic_string();
					RefreshDiscoveredLevelScenes();
					RegisterFallbackDefinitions();
					return false;
				}

				startupScene_ = registryJson.value("StartupScene", "TitleScene");
				debugStartupScene_ = registryJson.value("DebugStartupScene", startupScene_);
				const std::filesystem::path baseDirectory = registryPath.parent_path();

				if (registryJson.contains("Scenes") && registryJson["Scenes"].is_array())
				{
					for (const nlohmann::json& sceneEntry : registryJson["Scenes"])
					{
						if (!sceneEntry.is_string()) continue;
						LoadSceneFile(baseDirectory / sceneEntry.get<std::string>());
					}
				}

				RefreshDiscoveredLevelScenes(); // Level JSONだけでもDataDriven SceneとしてEditor/Runtimeへ自動登録する。
				if (definitions_.empty())
				{
					lastError_ = "Scene定義またはLevelが1件も読み込まれませんでした。";
					RegisterFallbackDefinitions();
					return false;
				}

				return lastError_.empty();
			}
			catch (const std::exception& exception)
			{
				lastError_ = std::string("Scene Registry読込中に例外が発生しました: ") + exception.what();
				RefreshDiscoveredLevelScenes();
				RegisterFallbackDefinitions();
				return false;
			}
		}

		void Clear()
		{
			definitions_.clear();
			startupScene_ = "TitleScene";
			debugStartupScene_ = "DebugScene";
			lastError_.clear();
			registryPath_.clear();
		}

		void RefreshDiscoveredLevelScenes() const
		{
			const std::filesystem::path sceneDirectory = registryPath_.empty()
				? std::filesystem::path("Resources/JSON/Scenes")
				: registryPath_.parent_path();
			const std::filesystem::path levelDirectory = sceneDirectory.parent_path() / "Levels";

			std::error_code error;
			if (!std::filesystem::exists(levelDirectory, error) || error) return;
			for (std::filesystem::directory_iterator iterator(levelDirectory, error), end; !error && iterator != end; iterator.increment(error))
			{
				const std::filesystem::directory_entry& entry = *iterator;
				if (!entry.is_regular_file(error) || error) continue;
				const std::filesystem::path path = entry.path();
				if (path.extension() != ".json") continue;

				const std::string sceneId = path.stem().string();
				if (sceneId.empty() || definitions_.contains(sceneId)) continue; // 明示Scene JSONがある場合はそちらを優先する。

				SceneDefinition definition{};
				definition.id = sceneId;
				definition.className = "DataDrivenScene";
				definition.levelPath = path.generic_string();
				definitions_.emplace(sceneId, std::move(definition));
			}
		}

		[[nodiscard]] const SceneDefinition* Find(const std::string& sceneId) const
		{
			RefreshDiscoveredLevelScenes(); // Save Level As直後でも次のScene検索から自動認識する。
			const auto iterator = definitions_.find(sceneId);
			return iterator != definitions_.end() ? &iterator->second : nullptr;
		}

		[[nodiscard]] std::string GetStartupScene(bool debugBuild) const
		{
			ProjectSettings* projectSettings = ProjectSettings::GetInstance();
			if (projectSettings->EnsureLoaded())
			{
				const std::string& overrideScene = projectSettings->GetStartupSceneOverride();
				if (!overrideScene.empty() && Find(overrideScene)) return overrideScene;
			}

			const std::string& requested = debugBuild ? debugStartupScene_ : startupScene_;
			if (Find(requested)) return requested;
			return definitions_.empty() ? requested : definitions_.begin()->first;
		}

		[[nodiscard]] const std::unordered_map<std::string, SceneDefinition>& GetDefinitions() const
		{
			RefreshDiscoveredLevelScenes();
			return definitions_;
		}
		[[nodiscard]] const std::string& GetLastError() const { return lastError_; }
		[[nodiscard]] const std::filesystem::path& GetRegistryPath() const { return registryPath_; }

	private:
		bool LoadSceneFile(const std::filesystem::path& path)
		{
			SceneDefinition definition{};
			std::string error;
			if (!SceneDefinitionSerializer::LoadFromFile(path, definition, error))
			{
				AppendError(std::move(error));
				return false;
			}

			definitions_[definition.id] = std::move(definition);
			return true;
		}

		void AppendError(std::string message)
		{
			if (!lastError_.empty()) lastError_ += "\n";
			lastError_ += std::move(message);
		}

		void RegisterFallbackDefinitions()
		{
			const std::vector<std::pair<std::string, bool>> fallbackScenes = {
				{ "TitleScene", false },
				{ "StageSelectScene", false },
				{ "GamePlayScene", false },
				{ "DebugScene", true },
			};
			for (const auto& [name, editorOnly] : fallbackScenes)
			{
				if (definitions_.contains(name)) continue;
				SceneDefinition definition{};
				definition.id = name;
				definition.className = editorOnly ? "DebugScene" : "DataDrivenScene";
				definition.editorOnly = editorOnly;
				definitions_.emplace(name, std::move(definition));
			}
		}

		mutable std::unordered_map<std::string, SceneDefinition> definitions_;
		std::string startupScene_ = "TitleScene";
		std::string debugStartupScene_ = "DebugScene";
		std::string lastError_;
		std::filesystem::path registryPath_;
	};
} // namespace Ken4lowEngine
