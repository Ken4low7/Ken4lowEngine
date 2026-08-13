#include "SceneDefinitionSerializer.h"

#include <fstream>
#include <utility>

namespace Ken4lowEngine
{
	bool SceneDefinitionSerializer::LoadFromFile(
		const std::filesystem::path& path,
		SceneDefinition& outDefinition,
		std::string& outError)
	{
		try
		{
			std::ifstream file(path);
			if (!file.is_open())
			{
				outError = "Scene定義を開けません: " + path.generic_string();
				return false;
			}

			nlohmann::json json;
			file >> json;
			return Deserialize(json, path, outDefinition, outError);
		}
		catch (const std::exception& exception)
		{
			outError = path.generic_string() + ": " + exception.what();
			return false;
		}
	}

	bool SceneDefinitionSerializer::Deserialize(
		const nlohmann::json& json,
		const std::filesystem::path& sourcePath,
		SceneDefinition& outDefinition,
		std::string& outError)
	{
		outError.clear();
		if (!json.is_object())
		{
			outError = "Scene定義のRootがobjectではありません: " + sourcePath.generic_string();
			return false;
		}

		// Phase 4以前のScene JSONはFormat / Versionを持たないためVersion 1として扱う。
		if (json.contains("Format") && json.value("Format", std::string{}) != "Ken4lowSceneDefinition")
		{
			outError = "Scene定義Formatが不正です: " + sourcePath.generic_string();
			return false;
		}
		const int version = json.value("Version", 1);
		if (version <= 0 || version > static_cast<int>(kCurrentVersion))
		{
			outError = "未対応のScene定義Versionです: " + std::to_string(version);
			return false;
		}

		SceneDefinition definition{};
		definition.id = json.value("Id", sourcePath.stem().string());
		definition.className = json.value("Class", std::string("DataDrivenScene")); // Class省略時は汎用ActorWorld Sceneとして扱う。
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
			outError = "必須項目が不足したScene定義です: " + sourcePath.generic_string();
			return false;
		}

		outDefinition = std::move(definition);
		return true;
	}
} // namespace Ken4lowEngine
