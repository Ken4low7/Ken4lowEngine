#pragma once

#include <Engine/Core/Streaming/StreamingManager.h>
#include <Engine/Scene/Actor/Core/ActorHandle.h>
#include <Engine/Scene/Level/LevelDocument.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ken4lowEngine
{
	class Actor;
	class ActorWorld;

	enum class SubLevelState : uint8_t
	{
		Unloaded,
		Loading,
		Loaded,
		Failed,
	};

	/// <summary>
	/// SubLevelを非同期で読み込み、Main ThreadでActorWorldへAdditive Commitする。
	/// SubLevel ActorはHandleで追跡し、Unload時に安全なDestroy予約へ変換する。
	/// </summary>
	class SubLevelManager
	{
	public:
		static SubLevelManager* GetInstance();

		void Configure(ActorWorld* actorWorld, const std::vector<LevelSubLevelReference>& subLevels);
		void Reset();

		bool RequestLoad(std::string_view id);
		bool RequestUnload(std::string_view id);
		bool Retry(std::string_view id);
		bool UpdateReferenceMetadata(const LevelSubLevelReference& reference);

		[[nodiscard]] SubLevelState GetState(std::string_view id) const;
		[[nodiscard]] const std::string& GetLastError(std::string_view id) const;
		[[nodiscard]] std::size_t GetLoadedSubLevelCount() const;
		[[nodiscard]] bool IsStreamingActor(const Actor* actor) const;
		[[nodiscard]] const std::vector<LevelSubLevelReference>& GetReferences() const { return references_; }
		[[nodiscard]] ActorWorld* GetActorWorld() const { return actorWorld_; }

	private:
		struct Entry
		{
			LevelSubLevelReference reference;
			SubLevelState state = SubLevelState::Unloaded;
			StreamingRequestHandle request;
			std::vector<ActorHandle> actors;
			uint64_t generation = 0;
			std::string lastError;
		};

		SubLevelManager() = default;
		SubLevelManager(const SubLevelManager&) = delete;
		SubLevelManager& operator=(const SubLevelManager&) = delete;

		void CompleteLoad(std::string id, uint64_t generation, StreamingManager::Payload&& payload, std::exception_ptr exception);
		void RemoveTrackedActors(Entry& entry);
		static StreamingPriority ToStreamingPriority(int priority);

		ActorWorld* actorWorld_ = nullptr;
		std::vector<LevelSubLevelReference> references_;
		std::unordered_map<std::string, Entry> entries_;
		std::unordered_set<uint64_t> streamedActorIds_;
		std::string emptyError_;
	};
} // namespace Ken4lowEngine
