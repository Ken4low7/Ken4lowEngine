#include "PrefabReferenceResolver.h"

#include "ActorJsonSerializer.h"

#include <fstream>

namespace Ken4lowEngine
{
	PrefabReferenceResolver::Result PrefabReferenceResolver::Resolve(
		std::string_view prefabPath,
		const nlohmann::json& overrides,
		const std::filesystem::path& levelBaseDirectory)
	{
		Result result{};
		std::string error;
		if (!LoadBaseActor(prefabPath, levelBaseDirectory, result.actorJson, error))
		{
			result.message = std::move(error);
			return result;
		}

		result.resolvedPath = ResolvePath(prefabPath, levelBaseDirectory);
		if (!overrides.is_null())
		{
			if (!overrides.is_object())
			{
				result.message = "Prefab OverridesのRootはobjectである必要があります: " + result.resolvedPath.generic_string();
				return result;
			}
			result.actorJson.merge_patch(overrides); // nullは削除、objectは再帰、配列と値は置換する。
		}

		if (!ActorJsonSerializer::ValidateActorDefinition(result.actorJson))
		{
			result.message = "Prefab Override適用後のActor定義が不正です: " + result.resolvedPath.generic_string();
			return result;
		}

		result.succeeded = true;
		result.message = "Prefabを解決しました: " + result.resolvedPath.generic_string();
		return result;
	}

	bool PrefabReferenceResolver::LoadBaseActor(
		std::string_view prefabPath,
		const std::filesystem::path& levelBaseDirectory,
		nlohmann::json& outActorJson,
		std::string& outError)
	{
		outActorJson = nlohmann::json::object();
		outError.clear();
		if (prefabPath.empty())
		{
			outError = "Prefab Pathが空です。";
			return false;
		}

		const std::filesystem::path resolvedPath = ResolvePath(prefabPath, levelBaseDirectory);
		try
		{
			std::ifstream file(resolvedPath);
			if (!file.is_open())
			{
				outError = "Prefabファイルを開けません: " + resolvedPath.generic_string();
				return false;
			}

			nlohmann::json json;
			file >> json;
			if (json.is_object() && json.value("Format", std::string{}) == "Ken4lowActorPrefab" &&
				json.contains("Actor") && json["Actor"].is_object())
			{
				outActorJson = json["Actor"]; // 将来のVersion付きPrefab wrapperにも互換対応する。
			}
			else
			{
				outActorJson = std::move(json); // 現行のActorPrefabはActor JSONそのものを保存している。
			}

			if (!ActorJsonSerializer::ValidateActorDefinition(outActorJson))
			{
				outError = "PrefabのActor定義が不正です: " + resolvedPath.generic_string();
				return false;
			}
			return true;
		}
		catch (const std::exception& exception)
		{
			outError = "Prefab読込中に例外が発生しました: " + std::string(exception.what());
			return false;
		}
	}

	nlohmann::json PrefabReferenceResolver::BuildOverrides(
		const nlohmann::json& baseActorJson,
		const nlohmann::json& instanceActorJson)
	{
		nlohmann::json patch = nlohmann::json::object();
		BuildOverridesRecursive(baseActorJson, instanceActorJson, patch);
		return patch;
	}

	std::filesystem::path PrefabReferenceResolver::ResolvePath(
		std::string_view prefabPath,
		const std::filesystem::path& levelBaseDirectory)
	{
		std::filesystem::path path{ std::string(prefabPath) };
		if (path.is_absolute()) return path.lexically_normal();

		std::error_code error;
		if (std::filesystem::exists(path, error)) return path.lexically_normal();
		if (!levelBaseDirectory.empty()) return (levelBaseDirectory / path).lexically_normal();
		return path.lexically_normal();
	}

	bool PrefabReferenceResolver::BuildOverridesRecursive(
		const nlohmann::json& baseValue,
		const nlohmann::json& instanceValue,
		nlohmann::json& outPatch)
	{
		if (baseValue == instanceValue) return false;

		if (baseValue.is_object() && instanceValue.is_object())
		{
			nlohmann::json patch = nlohmann::json::object();
			bool changed = false;

			for (auto baseIt = baseValue.begin(); baseIt != baseValue.end(); ++baseIt)
			{
				if (!instanceValue.contains(baseIt.key()))
				{
					patch[baseIt.key()] = nullptr; // Merge PatchのnullでPrefab側プロパティを削除する。
					changed = true;
				}
			}

			for (auto instanceIt = instanceValue.begin(); instanceIt != instanceValue.end(); ++instanceIt)
			{
				const auto baseIt = baseValue.find(instanceIt.key());
				if (baseIt == baseValue.end())
				{
					patch[instanceIt.key()] = instanceIt.value();
					changed = true;
					continue;
				}

				nlohmann::json childPatch;
				if (BuildOverridesRecursive(baseIt.value(), instanceIt.value(), childPatch))
				{
					patch[instanceIt.key()] = std::move(childPatch);
					changed = true;
				}
			}

			if (changed) outPatch = std::move(patch);
			return changed;
		}

		outPatch = instanceValue; // 配列とScalarはMerge Patch仕様どおり値全体を置換する。
		return true;
	}
} // namespace Ken4lowEngine
