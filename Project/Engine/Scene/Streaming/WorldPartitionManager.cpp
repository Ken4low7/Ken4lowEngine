#include "WorldPartitionManager.h"

#include "SubLevelManager.h"
#include <Vector3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Ken4lowEngine
{
	WorldPartitionManager* WorldPartitionManager::GetInstance()
	{
		static WorldPartitionManager instance;
		return &instance;
	}

	void WorldPartitionManager::Configure(
		ActorWorld* actorWorld,
		const LevelWorldPartitionSettings& settings,
		const std::vector<LevelSubLevelReference>& subLevels)
	{
		Reset();
		actorWorld_ = actorWorld;
		settings_ = settings;
		settings_.cellSize = (std::max)(1.0f, settings_.cellSize);
		settings_.loadRadiusCells = (std::max)(0, settings_.loadRadiusCells);
		settings_.unloadRadiusCells = (std::max)(settings_.loadRadiusCells, settings_.unloadRadiusCells);
		subLevels_ = subLevels;
		SubLevelManager::GetInstance()->Configure(actorWorld_, subLevels_);

		for (const LevelSubLevelReference& subLevel : subLevels_)
		{
			if (subLevel.alwaysLoaded) SubLevelManager::GetInstance()->RequestLoad(subLevel.id);
		}
	}

	void WorldPartitionManager::Reset()
	{
		SubLevelManager::GetInstance()->Reset();
		actorWorld_ = nullptr;
		settings_ = {};
		subLevels_.clear();
	}

	void WorldPartitionManager::Update(const Vector3& streamingSourcePosition)
	{
		if (!actorWorld_) return;
		if (!settings_.enabled)
		{
			for (const LevelSubLevelReference& subLevel : subLevels_)
			{
				if (subLevel.alwaysLoaded) SubLevelManager::GetInstance()->RequestLoad(subLevel.id);
			}
			return;
		}

		const int sourceCellX = static_cast<int>(std::floor(streamingSourcePosition.x / settings_.cellSize));
		const int sourceCellZ = static_cast<int>(std::floor(streamingSourcePosition.z / settings_.cellSize));

		for (const LevelSubLevelReference& subLevel : subLevels_)
		{
			if (subLevel.alwaysLoaded)
			{
				SubLevelManager::GetInstance()->RequestLoad(subLevel.id);
				continue;
			}

			const int cellDistance = (std::max)(
				std::abs(subLevel.cellX - sourceCellX),
				std::abs(subLevel.cellZ - sourceCellZ));
			if (cellDistance <= settings_.loadRadiusCells)
			{
				SubLevelManager::GetInstance()->RequestLoad(subLevel.id);
			}
			else if (cellDistance > settings_.unloadRadiusCells)
			{
				SubLevelManager::GetInstance()->RequestUnload(subLevel.id);
			}
		}
	}

	std::size_t WorldPartitionManager::GetLoadedSubLevelCount() const
	{
		return SubLevelManager::GetInstance()->GetLoadedSubLevelCount();
	}

	bool WorldPartitionManager::IsStreamingActor(const Actor* actor) const
	{
		return SubLevelManager::GetInstance()->IsStreamingActor(actor);
	}
} // namespace Ken4lowEngine
