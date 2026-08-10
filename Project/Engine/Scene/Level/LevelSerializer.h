#pragma once

#include "LevelDocument.h"

#include <filesystem>
#include <string>
#include <string_view>

namespace Ken4lowEngine
{
	class ActorWorld;

	/// <summary>LevelDocumentとKen4lowLevel JSONの相互変換を一元化する。</summary>
	class LevelSerializer
	{
	public:
		struct Result
		{
			bool succeeded = false;
			uint32_t sourceVersion = 0;
			bool migrated = false;
			std::string message;
		};

		static Result LoadFromFile(const std::filesystem::path& path, LevelDocument& outDocument);
		static Result SaveToFileAtomic(const std::filesystem::path& path, const LevelDocument& document);

		static bool Deserialize(
			const nlohmann::json& sourceJson,
			const std::filesystem::path& levelBaseDirectory,
			LevelDocument& outDocument,
			std::string& outError);

		static nlohmann::json Serialize(const LevelDocument& document);

		/// 現在のActorWorldをLevelDocumentへ変換する。Lighting / Cameraは呼び出し側から渡す。
		static LevelDocument CaptureWorld(
			const ActorWorld& actorWorld,
			std::string_view levelName,
			std::string_view targetScene,
			nlohmann::json lighting = nlohmann::json::object(),
			nlohmann::json camera = nlohmann::json::object(),
			nlohmann::json environment = nlohmann::json::object());

	private:
		static bool ValidateActorGraph(const LevelDocument& document, std::string& outError);
	};
} // namespace Ken4lowEngine
