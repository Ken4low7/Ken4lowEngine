#pragma once

#include "SceneDefinition.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Scene Registryと各Scene JSONを読み込み、Scene IDから定義を解決します。</summary>
	class SceneDefinitionRegistry
	{
	public:
		SceneDefinitionRegistry()
		{
			Load("Resources/JSON/Scenes/SceneRegistry.json"); // SceneManager生成時に標準Registryを自動読込して既存ChangeScene呼び出しをデータ化する。
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
					RegisterFallbackDefinitions();
					return false;
				}

				nlohmann::json registryJson;
				registryFile >> registryJson;
				if (!registryJson.is_object() || registryJson.value("Format", std::string{}) != "Ken4lowSceneRegistry")
				{
					lastError_ = "Scene Registry形式が不正です: " + registryPath.generic_string();
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

				if (definitions_.empty())
				{
					lastError_ = "Scene定義が1件も読み込まれませんでした。";
					RegisterFallbackDefinitions();
					return false;
				}

				return lastError_.empty(); // 一部失敗時も読めたSceneは利用し、警告だけ保持する。
			}
			catch (const std::exception& exception)
			{
				lastError_ = std::string("Scene Registry読込中に例外が発生しました: ") + exception.what();
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
			registryPath_.clear(); // 再読込時に前回のRegistry情報を残さない。
		}

		[[nodiscard]] const SceneDefinition* Find(const std::string& sceneId) const
		{
			const auto iterator = definitions_.find(sceneId);
			return iterator != definitions_.end() ? &iterator->second : nullptr;
		}

		[[nodiscard]] std::string GetStartupScene(bool debugBuild) const
		{
			const std::string& requested = debugBuild ? debugStartupScene_ : startupScene_;
			if (Find(requested)) return requested;
			return definitions_.empty() ? requested : definitions_.begin()->first;
		}

		[[nodiscard]] const std::unordered_map<std::string, SceneDefinition>& GetDefinitions() const { return definitions_; }
		[[nodiscard]] const std::string& GetLastError() const { return lastError_; }
		[[nodiscard]] const std::filesystem::path& GetRegistryPath() const { return registryPath_; }

	private:
		bool LoadSceneFile(const std::filesystem::path& path)
		{
			try
			{
				std::ifstream file(path);
				if (!file.is_open())
				{
					AppendError("Scene定義を開けません: " + path.generic_string());
					return false;
				}

				nlohmann::json json;
				file >> json;
				SceneDefinition definition{};
				definition.id = json.value("Id", path.stem().string());
				definition.className = json.value("Class", definition.id);
				definition.levelPath = json.value("Level", std::string{});
				definition.gameMode = json.value("GameMode", std::string{});
				definition.playerActor = json.value("PlayerActor", std::string{});
				definition.uiLayout = json.value("UILayout", std::string{});
				definition.bgmPath = json.value("BGM", std::string{});
				definition.nextScene = json.value("NextScene", std::string{});
				definition.retryScene = json.value("RetryScene", std::string{});
				definition.editorOnly = json.value("EditorOnly", false);

				if (json.contains("Transition") && json["Transition"].is_object())
				{
					definition.transition.type = json["Transition"].value("Type", "Fade");
					definition.transition.duration = json["Transition"].value("Duration", 1.0f);
				}
				if (json.contains("Parameters") && json["Parameters"].is_object())
				{
					definition.parameters = json["Parameters"];
				}

				if (!definition.IsValid())
				{
					AppendError("必須項目が不足したScene定義です: " + path.generic_string());
					return false;
				}

				definitions_[definition.id] = std::move(definition); // 同じIDは後から読んだ定義で明示的に上書きする。
				return true;
			}
			catch (const std::exception& exception)
			{
				AppendError(path.generic_string() + ": " + exception.what());
				return false;
			}
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
				definition.className = name;
				definition.editorOnly = editorOnly;
				definitions_.emplace(name, std::move(definition));
			}
		}

		std::unordered_map<std::string, SceneDefinition> definitions_;
		std::string startupScene_ = "TitleScene";
		std::string debugStartupScene_ = "DebugScene";
		std::string lastError_;
		std::filesystem::path registryPath_;
	};
} // namespace Ken4lowEngine
