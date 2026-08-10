#include "SubLevelManager.h"

#include <Engine/Scene/Actor/Core/Actor.h>
#include <Engine/Scene/Actor/Core/ActorSpawnOptions.h>
#include <Engine/Scene/Actor/Core/ActorWorld.h>
#include <Engine/Scene/Actor/Serialization/ActorJsonSerializer.h>
#include <Engine/Scene/Actor/Serialization/PrefabInstanceRegistry.h>
#include <Engine/Scene/Actor/Components/SceneComponent.h>
#include <Engine/Scene/Level/LevelSerializer.h>

#include <any>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		struct SubLevelFilePayload
		{
			std::filesystem::path path;
			std::string jsonText;
		};

		bool StageSubLevel(
			const LevelDocument& document,
			std::vector<std::unique_ptr<Actor>>& outActors,
			std::string& outError)
		{
			outActors.clear();
			outActors.reserve(document.actors.size());
			std::unordered_map<std::string, Actor*> actorsByDocumentId;

			ActorSpawnOptions spawnOptions{};
			spawnOptions.applySpawnOffset = false;
			spawnOptions.disableAutoRegisterMainCamera = true; // Streaming CellがMainCameraを奪わないようにする。

			for (const LevelActorDocument& actorDocument : document.actors)
			{
				std::unique_ptr<Actor> actor = ActorJsonSerializer::CreateActorFromJson(actorDocument.resolvedData, spawnOptions);
				if (!actor)
				{
					outError = "SubLevel ActorのStagingに失敗しました: " + actorDocument.id;
					return false;
				}
				actorsByDocumentId[actorDocument.id] = actor.get();
				outActors.push_back(std::move(actor));
			}

			for (const LevelActorDocument& actorDocument : document.actors)
			{
				if (actorDocument.parentId.empty()) continue;
				const auto child = actorsByDocumentId.find(actorDocument.id);
				const auto parent = actorsByDocumentId.find(actorDocument.parentId);
				if (child == actorsByDocumentId.end() || parent == actorsByDocumentId.end())
				{
					outError = "SubLevel内の親子関係を解決できません: " + actorDocument.id;
					return false;
				}
				SceneComponent* childRoot = child->second ? child->second->GetRootComponent() : nullptr;
				SceneComponent* parentRoot = parent->second ? parent->second->GetRootComponent() : nullptr;
				if (!childRoot || !parentRoot)
				{
					outError = "SubLevelの親子ActorにRootComponentがありません: " + actorDocument.id;
					return false;
				}
				childRoot->AttachTo(parentRoot);
			}
			return true;
		}

		void FinalizeStagedActors(std::vector<std::unique_ptr<Actor>>& actors)
		{
			for (std::unique_ptr<Actor>& actor : actors)
			{
				if (actor) actor->FinalizeForWorld();
			}
			actors.clear();
		}
	}

	SubLevelManager* SubLevelManager::GetInstance()
	{
		static SubLevelManager instance;
		return &instance;
	}

	void SubLevelManager::Configure(ActorWorld* actorWorld, const std::vector<LevelSubLevelReference>& subLevels)
	{
		Reset();
		actorWorld_ = actorWorld;
		references_ = subLevels;
		for (const LevelSubLevelReference& reference : references_)
		{
			Entry entry{};
			entry.reference = reference;
			entries_.emplace(reference.id, std::move(entry));
		}
	}

	void SubLevelManager::Reset()
	{
		for (auto& [id, entry] : entries_)
		{
			(void)id;
			entry.request.Cancel();
			++entry.generation; // Queue済みCompletionも旧世代として無効化する。
		}
		entries_.clear();
		references_.clear();
		streamedActorIds_.clear();
		actorWorld_ = nullptr;
	}

	bool SubLevelManager::RequestLoad(std::string_view id)
	{
		if (!actorWorld_) return false;
		const auto found = entries_.find(std::string(id));
		if (found == entries_.end()) return false;
		Entry& entry = found->second;
		if (entry.state == SubLevelState::Loading || entry.state == SubLevelState::Loaded) return true;

		const std::filesystem::path levelPath = entry.reference.resolvedPath.empty()
			? std::filesystem::path(entry.reference.path)
			: entry.reference.resolvedPath;
		if (levelPath.empty())
		{
			entry.state = SubLevelState::Failed;
			entry.lastError = "SubLevel Pathが空です: " + entry.reference.id;
			return false;
		}

		entry.lastError.clear();
		entry.state = SubLevelState::Loading;
		const uint64_t generation = ++entry.generation;
		const std::string entryId = entry.reference.id;

		entry.request = StreamingManager::GetInstance()->Request(
			[levelPath]() -> StreamingManager::Payload
			{
				std::ifstream file(levelPath, std::ios::binary);
				if (!file.is_open()) throw std::runtime_error("SubLevelファイルを開けません: " + levelPath.generic_string());
				std::ostringstream stream;
				stream << file.rdbuf();
				return SubLevelFilePayload{ levelPath, stream.str() };
			},
			[this, entryId, generation](StreamingManager::Payload&& payload, std::exception_ptr exception)
			{
				CompleteLoad(entryId, generation, std::move(payload), exception);
			},
			ToStreamingPriority(entry.reference.priority));

		if (!entry.request.IsValid())
		{
			entry.state = SubLevelState::Failed;
			entry.lastError = "SubLevel Streaming Requestの作成に失敗しました: " + entry.reference.id;
			return false;
		}
		return true;
	}

	bool SubLevelManager::RequestUnload(std::string_view id)
	{
		const auto found = entries_.find(std::string(id));
		if (found == entries_.end()) return false;
		Entry& entry = found->second;
		if (entry.state == SubLevelState::Unloaded) return true;

		entry.request.Cancel();
		++entry.generation;
		RemoveTrackedActors(entry);
		entry.state = SubLevelState::Unloaded;
		entry.lastError.clear();
		return true;
	}

	bool SubLevelManager::Retry(std::string_view id)
	{
		const auto found = entries_.find(std::string(id));
		if (found == entries_.end()) return false;
		if (found->second.state != SubLevelState::Failed) return RequestLoad(id);
		found->second.state = SubLevelState::Unloaded;
		return RequestLoad(id);
	}

	SubLevelState SubLevelManager::GetState(std::string_view id) const
	{
		const auto found = entries_.find(std::string(id));
		return found != entries_.end() ? found->second.state : SubLevelState::Unloaded;
	}

	const std::string& SubLevelManager::GetLastError(std::string_view id) const
	{
		const auto found = entries_.find(std::string(id));
		return found != entries_.end() ? found->second.lastError : emptyError_;
	}

	std::size_t SubLevelManager::GetLoadedSubLevelCount() const
	{
		std::size_t count = 0;
		for (const auto& [id, entry] : entries_)
		{
			(void)id;
			if (entry.state == SubLevelState::Loaded) ++count;
		}
		return count;
	}

	bool SubLevelManager::IsStreamingActor(const Actor* actor) const
	{
		// ActorWorldはWorldポインタを外部公開しないため、現在Worldで発行したActorIdの追跡集合だけを使う。
		// Configure/Reset時に集合を破棄するので、別Worldの同一IDへ状態を持ち越さない。
		return actor && actorWorld_ && actor->GetId().IsValid() &&
			streamedActorIds_.contains(actor->GetId().value);
	}

	void SubLevelManager::CompleteLoad(
		std::string id,
		uint64_t generation,
		StreamingManager::Payload&& payload,
		std::exception_ptr exception)
	{
		const auto found = entries_.find(id);
		if (found == entries_.end()) return;
		Entry& entry = found->second;
		if (generation != entry.generation || entry.state != SubLevelState::Loading) return;

		try
		{
			if (exception) std::rethrow_exception(exception);
			SubLevelFilePayload filePayload = std::any_cast<SubLevelFilePayload>(std::move(payload));
			nlohmann::json levelJson = nlohmann::json::parse(filePayload.jsonText);
			LevelDocument document{};
			std::string deserializeError;
			if (!LevelSerializer::Deserialize(levelJson, filePayload.path.parent_path(), document, deserializeError))
			{
				throw std::runtime_error(deserializeError);
			}

			std::vector<std::unique_ptr<Actor>> stagedActors;
			std::string stageError;
			if (!StageSubLevel(document, stagedActors, stageError))
			{
				FinalizeStagedActors(stagedActors);
				throw std::runtime_error(stageError);
			}

			std::vector<Actor*> committedActors;
			if (!actorWorld_ || !actorWorld_->AppendStagedActors(std::move(stagedActors), &committedActors))
			{
				FinalizeStagedActors(stagedActors);
				throw std::runtime_error("SubLevelのAdditive Commitに失敗しました: " + id);
			}

			entry.actors.clear();
			entry.actors.reserve(committedActors.size());
			for (std::size_t index = 0; index < committedActors.size(); ++index)
			{
				Actor* actor = committedActors[index];
				if (!actor) continue;
				ActorHandle handle = actorWorld_->MakeActorHandle(actor);
				entry.actors.push_back(handle);
				if (handle.GetId().IsValid()) streamedActorIds_.insert(handle.GetId().value);

				if (index < document.actors.size() && document.actors[index].prefab.IsSet())
				{
					PrefabInstanceRegistry::GetInstance()->Register(actor, document.actors[index].prefab.path);
				}
			}
			entry.state = SubLevelState::Loaded;
			entry.lastError.clear();
		}
		catch (const std::exception& loadException)
		{
			entry.state = SubLevelState::Failed;
			entry.lastError = loadException.what();
		}
		catch (...)
		{
			entry.state = SubLevelState::Failed;
			entry.lastError = "SubLevel読込中に不明な例外が発生しました: " + id;
		}
	}

	void SubLevelManager::RemoveTrackedActors(Entry& entry)
	{
		if (actorWorld_)
		{
			for (const ActorHandle& handle : entry.actors)
			{
				if (handle.GetId().IsValid()) streamedActorIds_.erase(handle.GetId().value);
				if (Actor* actor = actorWorld_->ResolveActor(handle)) actorWorld_->DestroyActor(actor);
			}
		}
		else
		{
			for (const ActorHandle& handle : entry.actors)
			{
				if (handle.GetId().IsValid()) streamedActorIds_.erase(handle.GetId().value);
			}
		}
		entry.actors.clear();
	}

	StreamingPriority SubLevelManager::ToStreamingPriority(int priority)
	{
		if (priority >= 3) return StreamingPriority::Critical;
		if (priority == 2) return StreamingPriority::High;
		if (priority <= 0) return StreamingPriority::Background;
		return StreamingPriority::Normal;
	}
} // namespace Ken4lowEngine
