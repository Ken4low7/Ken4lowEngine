#pragma once

#include <json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>プロジェクト全体で共有する起動設定とフォールバック設定を保持する。</summary>
	class ProjectSettings
	{
	public:
		static constexpr uint32_t kCurrentVersion = 1;

		static ProjectSettings* GetInstance()
		{
			static ProjectSettings instance;
			return &instance;
		}

		bool Load(const std::filesystem::path& path = "Resources/JSON/ProjectSettings.json")
		{
			lastError_.clear();
			try
			{
				std::ifstream file(path);
				if (!file.is_open())
				{
					lastError_ = "Project Settingsを開けません: " + path.generic_string();
					loaded_ = false;
					return false;
				}

				nlohmann::json json;
				file >> json;
				if (!json.is_object() || json.value("Format", std::string{}) != "Ken4lowProjectSettings")
				{
					lastError_ = "Project Settings形式が不正です: " + path.generic_string();
					loaded_ = false;
					return false;
				}

				const uint32_t version = json.value("Version", 0u);
				if (version == 0 || version > kCurrentVersion)
				{
					lastError_ = "未対応のProject Settings Versionです: " + std::to_string(version);
					loaded_ = false;
					return false;
				}

				projectName_ = json.value("ProjectName", "Ken4lowEngine");
				resourceRoot_ = json.value("ResourceRoot", "Resources");
				sceneRegistryPath_ = json.value("SceneRegistry", "Resources/JSON/Scenes/SceneRegistry.json");
				startupSceneOverride_ = json.value("StartupSceneOverride", std::string{});

				if (json.contains("FallbackAssets") && json["FallbackAssets"].is_object())
				{
					const nlohmann::json& fallback = json["FallbackAssets"];
					fallbackTextureKey_ = fallback.value("TextureKey", "__Ken4lowMissingTexture");
					fallbackModelKey_ = fallback.value("ModelKey", "__Ken4lowMissingModel");
					fallbackAudioMode_ = fallback.value("AudioMode", "Silent");
				}

				loadedPath_ = path;
				loaded_ = true;
				return true;
			}
			catch (const std::exception& exception)
			{
				lastError_ = std::string("Project Settings読込中に例外が発生しました: ") + exception.what();
				loaded_ = false;
				return false;
			}
		}

		bool EnsureLoaded()
		{
			return loaded_ || Load();
		}

		[[nodiscard]] bool IsLoaded() const { return loaded_; }
		[[nodiscard]] const std::string& GetProjectName() const { return projectName_; }
		[[nodiscard]] const std::string& GetResourceRoot() const { return resourceRoot_; }
		[[nodiscard]] const std::string& GetSceneRegistryPath() const { return sceneRegistryPath_; }
		[[nodiscard]] const std::string& GetStartupSceneOverride() const { return startupSceneOverride_; }
		[[nodiscard]] const std::string& GetFallbackTextureKey() const { return fallbackTextureKey_; }
		[[nodiscard]] const std::string& GetFallbackModelKey() const { return fallbackModelKey_; }
		[[nodiscard]] const std::string& GetFallbackAudioMode() const { return fallbackAudioMode_; }
		[[nodiscard]] const std::string& GetLastError() const { return lastError_; }
		[[nodiscard]] const std::filesystem::path& GetLoadedPath() const { return loadedPath_; }

	private:
		ProjectSettings() = default;

		bool loaded_ = false;
		std::string projectName_ = "Ken4lowEngine";
		std::string resourceRoot_ = "Resources";
		std::string sceneRegistryPath_ = "Resources/JSON/Scenes/SceneRegistry.json";
		std::string startupSceneOverride_;
		std::string fallbackTextureKey_ = "__Ken4lowMissingTexture";
		std::string fallbackModelKey_ = "__Ken4lowMissingModel";
		std::string fallbackAudioMode_ = "Silent";
		std::string lastError_;
		std::filesystem::path loadedPath_;
	};
} // namespace Ken4lowEngine
