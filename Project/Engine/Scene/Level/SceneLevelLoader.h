#pragma once

#include "TransactionalLevelLoader.h"

namespace Ken4lowEngine
{
	/// <summary>既存BaseScene APIを維持しながらPhase 4 Transactional Loaderへ転送する互換Facade。</summary>
	class SceneLevelLoader
	{
	public:
		using Result = TransactionalLevelLoader::Result;

		static Result Load(const std::filesystem::path& levelPath, ActorWorld& actorWorld)
		{
			return TransactionalLevelLoader::Load(levelPath, actorWorld);
		}
	};
} // namespace Ken4lowEngine
