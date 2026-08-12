#include "WorldPartitionManager.h"

#include "SubLevelManager.h"
#include <Vector3.h>

#include <algorithm>

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
		SanitizeSettings();
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
		lastStreamingSourcePosition_ = {};
		lastStreamingSourceCell_ = {};
	}

	void WorldPartitionManager::Update(const Vector3& streamingSourcePosition)
	{
		if (!actorWorld_) return;
		lastStreamingSourcePosition_ = streamingSourcePosition;
		lastStreamingSourceCell_ = WorldPartitionGrid::WorldToCell(
			streamingSourcePosition.x,
			streamingSourcePosition.z,
			settings_.cellSize); // RuntimeとEditorのSource Cell表示を同じ変換関数へ固定する。

		if (!settings_.enabled)
		{
			for (const LevelSubLevelReference& subLevel : subLevels_)
			{
				if (subLevel.alwaysLoaded) SubLevelManager::GetInstance()->RequestLoad(subLevel.id);
			}
			return;
		}

		for (const LevelSubLevelReference& subLevel : subLevels_)
		{
			const WorldPartitionCell targetCell{ subLevel.cellX, subLevel.cellZ };
			switch (WorldPartitionGrid::Evaluate(
				lastStreamingSourceCell_,
				targetCell,
				subLevel.alwaysLoaded,
				settings_.loadRadiusCells,
				settings_.unloadRadiusCells))
			{
			case WorldPartitionStreamingDecision::AlwaysLoaded:
			case WorldPartitionStreamingDecision::Load:
				SubLevelManager::GetInstance()->RequestLoad(subLevel.id);
				break;
			case WorldPartitionStreamingDecision::Unload:
				SubLevelManager::GetInstance()->RequestUnload(subLevel.id);
				break;
			case WorldPartitionStreamingDecision::Retain:
			default:
				break;
			}
		}
	}

	void WorldPartitionManager::ApplyEditorSettings(const LevelWorldPartitionSettings& settings)
	{
		if (!actorWorld_) return;
		settings_ = settings;
		SanitizeSettings();
		Update(lastStreamingSourcePosition_);
	}

	bool WorldPartitionManager::UpdateSubLevelEditorMetadata(
		std::string_view id,
		int cellX,
		int cellZ,
		int priority,
		bool alwaysLoaded)
	{
		if (!actorWorld_) return false;
		const auto found = std::find_if(subLevels_.begin(), subLevels_.end(), [id](const LevelSubLevelReference& reference)
			{
				return reference.id == id;
			});
		if (found == subLevels_.end()) return false;

		found->cellX = cellX;
		found->cellZ = cellZ;
		found->priority = (std::clamp)(priority, 0, 3);
		found->alwaysLoaded = alwaysLoaded;
		if (!SubLevelManager::GetInstance()->UpdateReferenceMetadata(*found)) return false;
		if (found->alwaysLoaded) SubLevelManager::GetInstance()->RequestLoad(found->id);
		Update(lastStreamingSourcePosition_);
		return true;
	}

	std::size_t WorldPartitionManager::GetLoadedSubLevelCount() const
	{
		return SubLevelManager::GetInstance()->GetLoadedSubLevelCount();
	}

	bool WorldPartitionManager::IsStreamingActor(const Actor* actor) const
	{
		return SubLevelManager::GetInstance()->IsStreamingActor(actor);
	}

	void WorldPartitionManager::SanitizeSettings()
	{
		settings_.cellSize = WorldPartitionGrid::SanitizeCellSize(settings_.cellSize);
		settings_.loadRadiusCells = WorldPartitionGrid::SanitizeLoadRadius(settings_.loadRadiusCells);
		settings_.unloadRadiusCells = WorldPartitionGrid::SanitizeUnloadRadius(
			settings_.loadRadiusCells,
			settings_.unloadRadiusCells);
	}
} // namespace Ken4lowEngine
