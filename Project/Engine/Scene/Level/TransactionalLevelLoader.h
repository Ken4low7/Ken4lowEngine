#pragma once

#include "LevelDocument.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

namespace Ken4lowEngine
{
	class ActorWorld;

	/// <summary>
	/// LevelDocumentを一度Stagingして完全に構築できることを確認してからActorWorldへCommitする。
	/// Staging失敗時は現在のActorWorldを変更しない。
	/// </summary>
	class TransactionalLevelLoader
	{
	public:
		struct Result
		{
			bool succeeded = false;
			std::size_t actorCount = 0;
			bool migrated = false;
			uint32_t sourceVersion = 0;
			std::string message;
		};

		static Result Load(const std::filesystem::path& levelPath, ActorWorld& actorWorld);
		static Result LoadDocument(const LevelDocument& document, ActorWorld& actorWorld);
	};
} // namespace Ken4lowEngine
