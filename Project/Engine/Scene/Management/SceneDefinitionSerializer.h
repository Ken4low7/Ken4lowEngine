#pragma once

#include "SceneDefinition.h"

#include <filesystem>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>SceneDefinition JSONの解釈をRegistryから分離し、形式検証を一元化する。</summary>
	class SceneDefinitionSerializer
	{
	public:
		static constexpr uint32_t kCurrentVersion = 1;

		static bool LoadFromFile(const std::filesystem::path& path, SceneDefinition& outDefinition, std::string& outError);
		static bool Deserialize(
			const nlohmann::json& json,
			const std::filesystem::path& sourcePath,
			SceneDefinition& outDefinition,
			std::string& outError);
	};
} // namespace Ken4lowEngine
