#pragma once

#include <json.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace Ken4lowEngine
{
	/// <summary>Prefab JSONを読み込み、Level側Overrideを適用して最終Actor JSONへ解決する。</summary>
	class PrefabReferenceResolver
	{
	public:
		struct Result
		{
			bool succeeded = false;
			std::filesystem::path resolvedPath;
			nlohmann::json actorJson = nlohmann::json::object();
			std::string message;
		};

		static Result Resolve(
			std::string_view prefabPath,
			const nlohmann::json& overrides,
			const std::filesystem::path& levelBaseDirectory = {});

		static bool LoadBaseActor(
			std::string_view prefabPath,
			const std::filesystem::path& levelBaseDirectory,
			nlohmann::json& outActorJson,
			std::string& outError);

		/// RFC 7396相当のJSON Merge Patchとして、Prefab baseからInstanceへの最小差分を生成する。
		static nlohmann::json BuildOverrides(const nlohmann::json& baseActorJson, const nlohmann::json& instanceActorJson);

	private:
		static std::filesystem::path ResolvePath(std::string_view prefabPath, const std::filesystem::path& levelBaseDirectory);
		static bool BuildOverridesRecursive(const nlohmann::json& baseValue, const nlohmann::json& instanceValue, nlohmann::json& outPatch);
	};
} // namespace Ken4lowEngine
