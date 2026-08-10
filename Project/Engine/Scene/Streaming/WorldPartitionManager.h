#pragma once

#include <Engine/Scene/Level/LevelDocument.h>

#include <cstddef>
#include <vector>

class Vector3;

namespace Ken4lowEngine
{
	class Actor;
	class ActorWorld;

	/// <summary>
	/// Camera等のStreaming Source位置からCell距離を計算し、SubLevelのLoad/Unloadを制御する。
	/// Unreal EngineのWorld Partitionそのものではなく、Ken4lowEngine向けの軽量Grid Streaming基盤。
	/// </summary>
	class WorldPartitionManager
	{
	public:
		static WorldPartitionManager* GetInstance();

		void Configure(
			ActorWorld* actorWorld,
			const LevelWorldPartitionSettings& settings,
			const std::vector<LevelSubLevelReference>& subLevels);
		void Reset();
		void Update(const Vector3& streamingSourcePosition);

		[[nodiscard]] bool IsConfigured() const { return actorWorld_ != nullptr; }
		[[nodiscard]] bool IsEnabled() const { return settings_.enabled; }
		[[nodiscard]] bool IsConfiguredFor(const ActorWorld* actorWorld) const { return actorWorld_ == actorWorld; }
		[[nodiscard]] const LevelWorldPartitionSettings& GetSettings() const { return settings_; }
		[[nodiscard]] const std::vector<LevelSubLevelReference>& GetSubLevels() const { return subLevels_; }
		[[nodiscard]] std::size_t GetLoadedSubLevelCount() const;
		[[nodiscard]] bool IsStreamingActor(const Actor* actor) const;

	private:
		WorldPartitionManager() = default;
		WorldPartitionManager(const WorldPartitionManager&) = delete;
		WorldPartitionManager& operator=(const WorldPartitionManager&) = delete;

		ActorWorld* actorWorld_ = nullptr;
		LevelWorldPartitionSettings settings_{};
		std::vector<LevelSubLevelReference> subLevels_;
	};
} // namespace Ken4lowEngine
