#include "ActorWorld.h"

#include "ActorJsonSerializer.h"
#include "PrefabInstanceRegistry.h"
#include "CameraComponent.h"

#include "LightManager.h"
#include "SceneComponent.h"
#include <Engine/Scene/Streaming/WorldPartitionManager.h>

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#include <Editor/EditorContext.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstdio>

namespace Ken4lowEngine
{
	namespace
	{
		std::atomic_uint64_t gNextActorWorldId{ 1 };
	}

	void ActorWorld::Initialize()
	{
		if (isInitialized_)
		{
			return; // すでに初期化済みなら何もしない
		}

		for (auto& actor : actors_)
		{
			// 初期化処理は各Actorに委譲する
			actor->InitializeForWorld();

			// Actor初期化後に生成されたCollider / RigidbodyをPhysicsWorldへ登録する
			RegisterPhysicsComponents(*actor);
		}


		isInitialized_ = true; // ActorWorldの初期化が完了したことを示す
	}

	void ActorWorld::Update(float deltaTime)
	{
		ProcessPendingActorReload(); // JSON読込予約があれば次フレームの安全なタイミングで処理する
		ProcessPendingActorSpawn();	 // JSON生成予約があれば次フレームの安全なタイミングで処理する

		isUpdating_ = true;
		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy())
			{
				continue; // 削除予約済みActorは同じフレームで再更新しない。
			}

			if (!actor->IsActive()) continue;
			actor->InitializeComponents(); // 初期化完了通知でPhysics登録をイベント同期する。

			// 毎フレームのPhysics再走査は行わず、Component追加・削除・Active変更イベントで同期する。
			actor->Update(deltaTime);
			actor->InitializeComponents(); // Update中に追加されたComponentも同フレームのPhysics Step前に初期化する。

		}
		isUpdating_ = false;

		FlushPendingDestroyedActors();
		ProcessPendingActorAdds();

		SyncLightComponentsToLightManager(); // ActorのLightComponentを描画用ライトへ反映する
	}

	void ActorWorld::UpdateEditor(float deltaTime)
	{
		ProcessPendingActorReload();
		ProcessPendingActorSpawn();

		isUpdating_ = true;
		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy()) continue;
			if (!actor->IsActive()) continue;
			actor->InitializeComponents();
			actor->UpdateEditor(deltaTime);
		}
		isUpdating_ = false;

		FlushPendingDestroyedActors();
		ProcessPendingActorAdds();
		SyncLightComponentsToLightManager();
	}

	void ActorWorld::PostPhysicsUpdate(float deltaTime)
	{
		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive())
			{
				continue; // 無効なActorは物理後更新対象から外す
			}

			// PhysicsWorld更新後の処理をActorへ流す。
			actor->PostPhysicsUpdate(deltaTime);
		}

		SyncLightComponentsToLightManager(); // 物理更新後のWorld位置をライトへ反映する
	}

	void ActorWorld::Finalize()
	{
		WorldPartitionManager* partitionManager = WorldPartitionManager::GetInstance();
		if (partitionManager->IsConfiguredFor(this))
		{
			partitionManager->Reset(); // World破棄後に旧SubLevel Requestを残さない。
		}

		isUpdating_ = false;
		selectedActor_ = nullptr;
		selectedComponent_ = nullptr;
		pendingReloadActor_.Reset();
		hasPendingReloadActor_ = false;
		hasPendingSpawnActor_ = false;
		pendingReloadFilePath_.clear();
		pendingSpawnFilePath_.clear();

		for (auto& actor : actors_)
		{
			// Actor破棄前にPhysicsWorldが持つ外部参照を解除する
			UnregisterPhysicsComponents(*actor);

			// Actor破棄前にComponent側のFinalizeまで流す
			actor->FinalizeForWorld();
			ReleaseActorFromWorld(*actor);
		}

		actors_.clear(); // Finalize後にActorを破棄し、古い状態が残らないようにする
		for (PendingActorAdd& pending : pendingActorAdds_)
		{
			if (!pending.actor) continue;
			pending.actor->FinalizeForWorld();
			ReleaseActorFromWorld(*pending.actor);
		}
		pendingActorAdds_.clear();
		actorsById_.clear();
		componentsById_.clear();
		LightManager::GetInstance()->SetLightComponentLights({}); // ActorWorld破棄時にComponent由来ライトを解除する
#ifdef USE_IMGUI
		EditorActorStateRegistry::GetInstance()->Clear();
		EditorContext::GetInstance()->GetSelection().Clear();
#endif

		isInitialized_ = false; // 再Initialize時にSpawn済みActorを通常初期化できるように戻す
	}

	bool ActorWorld::CommitStagedActors(
		std::vector<std::unique_ptr<Actor>> stagedActors,
		std::vector<Actor*>* outActors)
	{
		if (isUpdating_) return false;
		for (const std::unique_ptr<Actor>& actor : stagedActors)
		{
			if (!actor) return false; // Destructive pointへ入る前に全Staging所有権を検証する。
		}

		if (outActors) outActors->clear();
		SetSelectedEditorObject(nullptr, nullptr);
		Finalize();
		Initialize(); // ここから先は検証済みActorの所有権移動だけなので失敗経路を持たせない。

		if (outActors) outActors->reserve(stagedActors.size());
		for (std::unique_ptr<Actor>& actor : stagedActors)
		{
			Actor* committedActor = AddActorToWorld(std::move(actor), false);
			if (outActors) outActors->push_back(committedActor);
		}
		return true;
	}

	bool ActorWorld::AppendStagedActors(
		std::vector<std::unique_ptr<Actor>> stagedActors,
		std::vector<Actor*>* outActors)
	{
		if (isUpdating_) return false;
		for (const std::unique_ptr<Actor>& actor : stagedActors)
		{
			if (!actor) return false;
		}

		if (outActors)
		{
			outActors->clear();
			outActors->reserve(stagedActors.size());
		}
		for (std::unique_ptr<Actor>& actor : stagedActors)
		{
			Actor* committedActor = AddActorToWorld(std::move(actor), false);
			if (outActors) outActors->push_back(committedActor);
		}
		return true;
	}

	Actor* ActorWorld::FindActorById(ActorId id)
	{
		if (!id.IsValid()) return nullptr;
		const auto found = actorsById_.find(id.value);
		return found != actorsById_.end() ? found->second : nullptr;
	}

	const Actor* ActorWorld::FindActorById(ActorId id) const
	{
		if (!id.IsValid()) return nullptr;
		const auto found = actorsById_.find(id.value);
		return found != actorsById_.end() ? found->second : nullptr;
	}

	ActorComponent* ActorWorld::FindComponentById(ComponentId id)
	{
		if (!id.IsValid()) return nullptr;
		const auto found = componentsById_.find(id.value);
		return found != componentsById_.end() ? found->second : nullptr;
	}

	const ActorComponent* ActorWorld::FindComponentById(ComponentId id) const
	{
		if (!id.IsValid()) return nullptr;
		const auto found = componentsById_.find(id.value);
		return found != componentsById_.end() ? found->second : nullptr;
	}

	ActorHandle ActorWorld::MakeActorHandle(const Actor* actor) const
	{
		return actor && OwnsActor(actor) && worldId_.IsValid()
			? ActorHandle(worldId_, actor->GetId())
			: ActorHandle{};
	}

	Actor* ActorWorld::ResolveActor(const ActorHandle& handle)
	{
		if (!handle.IsSet() || handle.GetWorldId() != worldId_) return nullptr;
		Actor* actor = FindActorById(handle.GetId());
		return actor && !actor->IsPendingDestroy() ? actor : nullptr; // Destroy予約した時点で外部Handleからは無効として扱う。
	}

	const Actor* ActorWorld::ResolveActor(const ActorHandle& handle) const
	{
		if (!handle.IsSet() || handle.GetWorldId() != worldId_) return nullptr;
		const Actor* actor = FindActorById(handle.GetId());
		return actor && !actor->IsPendingDestroy() ? actor : nullptr; // World終端の実破棄待ちでも参照を再取得させない。
	}

	bool ActorWorld::IsActorHandleValid(const ActorHandle& handle) const
	{
		return ResolveActor(handle) != nullptr;
	}

	Actor* ActorWorld::FindActorByName(std::string_view name, bool includeInactive)
	{
		for (auto& actor : actors_)
		{
			if (!actor || (!includeInactive && !actor->IsActive()))
			{
				continue; // 無効なActorを除外する指定なら検索対象から外す
			}

			// Actor名が一致した最初のActorを返す
			if (actor->GetName() == name)
			{
				return actor.get();
			}
		}

		return nullptr; // 名前が一致するActorが見つからなかった場合はnullptrを返す
	}

	std::vector<Actor*> ActorWorld::FindActorsWithTag(std::string_view tag, bool includeInactive)
	{
		std::vector<Actor*> results;
		if (tag.empty())
		{
			return results; // 空Tagでは検索しない
		}

		const std::string tagString{ tag };
		for (auto& actor : actors_)
		{
			if (!actor || (!includeInactive && !actor->IsActive()))
			{
				continue; // nullptrや除外対象のInactive Actorは無視する
			}

			if (actor->HasTag(tagString))
			{
				results.push_back(actor.get());
			}
		}

		return results;
	}

	std::vector<Actor*> ActorWorld::FindActorsByLayer(std::string_view layer, bool includeInactive)
	{
		std::vector<Actor*> results;
		const std::string layerString = layer.empty() ? "Default" : std::string(layer);

		for (auto& actor : actors_)
		{
			if (!actor || (!includeInactive && !actor->IsActive()))
			{
				continue; // nullptrや除外対象のInactive Actorは無視する
			}

			if (actor->GetLayer() == layerString)
			{
				results.push_back(actor.get());
			}
		}

		return results;
	}

	Actor* ActorWorld::SpawnActorFromJson(std::string_view filePath, const ActorSpawnOptions& options)
	{
		std::unique_ptr<Actor> actor = ActorJsonSerializer::CreateActorFromJson(filePath, options);
		if (!actor)
		{
			lastActorJsonSaveMessage_ = "Load failed : " + std::string(filePath);
			return nullptr; // JSON読み込みに失敗した場合はnullptrを返す
		}

		Actor* spawnedActor = actor.get(); // Spawn後も呼び出し側が生成したActorを扱えるようにポインタを保持しておく

		for (CameraComponent* cameraComponent : spawnedActor->GetComponents<CameraComponent>())
		{
			if (!cameraComponent)
			{
				continue; // CameraComponentがnullptrなら登録しない
			}

			if (options.disableAutoRegisterMainCamera)
			{
				cameraComponent->SetAutoRegisterMainCamera(false); // Prefab配置時は既存MainCameraを維持する。
			}
		}

		AddActorToWorld(std::move(actor), false); // JSON生成済みComponentはSerializer側で初期化されている。
		if (options.trackPrefabReference)
		{
			PrefabInstanceRegistry::GetInstance()->Register(spawnedActor, filePath);
		}

		selectedActor_ = spawnedActor; // Spawn後は自動的に生成したActorを選択状態にする
		selectedComponent_ = nullptr; // Spawn後はComponent選択を解除する

		lastActorJsonSaveMessage_ = "Spawned : " + std::string(filePath); // Spawn成功メッセージを更新する
		return spawnedActor;
	}

	bool ActorWorld::SaveActorToJson(const Actor& actor, std::string_view filePath)
	{
		if (!OwnsActor(&actor) || actor.IsPendingDestroy()) return false;
		return ActorJsonSerializer::SaveActorToFile(actor, filePath);
	}

	bool ActorWorld::DestroyActor(Actor* actor)
	{
		if (!OwnsActor(actor) || !actor || actor->IsPendingDestroy()) return false;
		actor->Destroy();
		return true;
	}

	bool ActorWorld::GetSelectedFocusPosition(Vector3& outPosition) const
	{
		if (selectedComponent_)
		{
			if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(selectedComponent_))
			{
				outPosition = sceneComponent->GetWorldPosition();
				return true;
			}

			if (Actor* owner = selectedComponent_->GetOwner())
			{
				if (SceneComponent* root = owner->GetRootComponent())
				{
					outPosition = root->GetWorldPosition(); // ActorComponentなら所有ActorのRoot位置を使う
					return true;
				}
			}
		}

		if (selectedActor_)
		{
			if (SceneComponent* root = selectedActor_->GetRootComponent())
			{
				outPosition = root->GetWorldPosition(); // Actor選択時はRoot位置を使う
				return true;
			}
		}

		// フォーカスできる対象がない
		return false;
	}

	void ActorWorld::ProcessPendingActorReload()
	{
		if (!hasPendingReloadActor_) return;
		Actor* reloadActor = ResolveActor(pendingReloadActor_);
		if (!reloadActor)
		{
			hasPendingReloadActor_ = false;
			pendingReloadActor_.Reset();
			pendingReloadFilePath_.clear();
			return; // Destroy済み・別World・無効IDへの遅延参照は次フレームへ持ち越さない。
		}

		const std::string reloadFilePath = pendingReloadFilePath_;

		hasPendingReloadActor_ = false; // 次フレームで再度処理されないようにフラグを下ろす
		pendingReloadActor_.Reset(); // 次フレームで再度処理されないようにHandleをクリアする
		pendingReloadFilePath_.clear(); // 次フレームで再度処理されないようにファイルパスをクリアする

		selectedComponent_ = nullptr; // Componentは作り直されるので選択解除する

		const bool succeeded = ReloadActorFromJson(*reloadActor, reloadFilePath);

		lastActorJsonSaveMessage_ = succeeded
			? "Loaded : " + reloadFilePath
			: "Load failed : " + reloadFilePath;

		selectedActor_ = reloadActor; // 読み込み後もActorを選択状態に戻す
	}

	bool ActorWorld::ReloadActorFromJson(Actor& actor, std::string_view filePath)
	{
		if (!OwnsActor(&actor) || actor.IsPendingDestroy()) return false;
		if (selectedComponent_ && selectedComponent_->GetOwner() == &actor) selectedComponent_ = nullptr;
#ifdef USE_IMGUI
		EditorContext::GetInstance()->GetSelection().Clear(); // 再生成されるComponentへの編集コールバックを先に破棄する。
#endif
		const nlohmann::json backup = ActorJsonSerializer::SerializeActor(actor);
		UnregisterPhysicsComponents(actor); // JSON読み込み前にPhysicsWorld登録を解除する

		bool succeeded = false;
		try
		{
			succeeded = ActorJsonSerializer::LoadActorFromFile(actor, filePath);
		}
		catch (...)
		{
			succeeded = false; // 壊れたJSONの例外をScene更新まで伝播させず、直前状態へ戻す。
		}
		if (!succeeded)
		{
			ActorJsonSerializer::LoadActorFromJson(actor, backup); // 読込途中で構成が変わっても直前の完全な状態を復元する。
			RegisterPhysicsComponents(actor); // JSON読み込みに失敗した場合はPhysicsWorld登録を元に戻す
			return false; // JSON読み込みに失敗した場合はfalseを返す
		}

		RegisterPhysicsComponents(actor); // JSON読み込み後にPhysicsWorld登録を再度行う
		return true;
	}

	void ActorWorld::ProcessPendingActorSpawn()
	{
		if (!hasPendingSpawnActor_)
		{
			return; // JSON生成予約が無い場合は何もしない
		}

		const std::string spawnFilePath = pendingSpawnFilePath_;
		const ActorSpawnOptions spawnOptions = pendingSpawnOptions_;

		hasPendingSpawnActor_ = false; // 次フレームで再度処理されないようにフラグを下ろす
		pendingSpawnFilePath_.clear(); //次フレームで再度処理されないようにファイルパスをクリアする
		pendingSpawnOptions_ = ActorSpawnOptions{}; // 次フレームで再度処理されないようにオプションをクリアする

		SpawnActorFromJson(spawnFilePath, spawnOptions); // JSONからActorを生成してActorWorldに追加する
	}

	Actor* ActorWorld::AddActorToWorld(std::unique_ptr<Actor> actor, bool initializeActor)
	{
		if (!actor) return nullptr;
		Actor* spawnedActor = actor.get();
		spawnedActor->SetName(MakeUniqueActorName(spawnedActor->GetName())); // 全生成経路でActor名の一意性を保証する。
		PrepareActorForWorld(*spawnedActor);
		if (isUpdating_)
		{
			pendingActorAdds_.push_back({ std::move(actor), initializeActor });
			return spawnedActor;
		}

		actors_.push_back(std::move(actor));
		if (isInitialized_ && initializeActor) spawnedActor->InitializeForWorld();
		if (isInitialized_) RegisterPhysicsComponents(*spawnedActor);
		return spawnedActor;
	}

	void ActorWorld::PrepareActorForWorld(Actor& actor)
	{
		if (!worldId_.IsValid())
		{
			worldId_ = WorldId{ gNextActorWorldId.fetch_add(1, std::memory_order_relaxed) };
		}

		ActorId actorId = actor.GetId();
		if (!actorId.IsValid() || (actorsById_.contains(actorId.value) && actorsById_[actorId.value] != &actor))
		{
			actorId = ActorId{ nextActorId_++ };
			actor.SetId(actorId);
		}
		actorsById_[actorId.value] = &actor;
		actor.SetWorld(this);
		for (const auto& component : actor.GetComponents())
		{
			if (component) OnComponentAdded(actor, *component);
		}
	}

	void ActorWorld::ReleaseActorFromWorld(Actor& actor)
	{
		PrefabInstanceRegistry::GetInstance()->Remove(&actor); // World寿命を越えてPrefab元ポインタを保持しない。
		for (const auto& component : actor.GetComponents())
		{
			if (!component) continue;
			if (component->GetId().IsValid()) componentsById_.erase(component->GetId().value);
			component->SetId({});
		}
		if (actor.GetId().IsValid()) actorsById_.erase(actor.GetId().value);
		actor.SetWorld(nullptr);
		actor.SetId({});
	}

	void ActorWorld::OnComponentAdded(Actor& actor, ActorComponent& component)
	{
		ComponentId componentId = component.GetId();
		if (!componentId.IsValid() || (componentsById_.contains(componentId.value) && componentsById_[componentId.value] != &component))
		{
			componentId = ComponentId{ nextComponentId_++ };
			component.SetId(componentId);
		}
		componentsById_[componentId.value] = &component;
		if (isInitialized_ && component.IsInitialized()) RegisterPhysicsComponents(actor);
	}

	void ActorWorld::OnComponentRemoving(Actor& actor, ActorComponent& component)
	{
		if (isInitialized_) UnregisterPhysicsComponents(actor);
		if (component.GetId().IsValid()) componentsById_.erase(component.GetId().value);
		component.SetId({});
	}

	void ActorWorld::OnComponentRuntimeStateChanged(Actor& actor, ActorComponent&)
	{
		if (!isInitialized_) return;
		RegisterPhysicsComponents(actor);
	}

	void ActorWorld::OnActorRuntimeStateChanged(Actor& actor)
	{
		if (!isInitialized_) return;
		if (actor.IsActive() && !actor.IsPendingDestroy()) RegisterPhysicsComponents(actor);
		else UnregisterPhysicsComponents(actor);
	}

	void ActorWorld::ProcessPendingActorAdds()
	{
		if (pendingActorAdds_.empty()) return;
		std::vector<PendingActorAdd> pendingAdds = std::move(pendingActorAdds_);
		pendingActorAdds_.clear();
		for (PendingActorAdd& pending : pendingAdds)
		{
			AddActorToWorld(std::move(pending.actor), pending.initializeActor);
		}
	}

	bool ActorWorld::OwnsActor(const Actor* actor) const
	{
		if (!actor) return false;
		const bool owned = std::any_of(actors_.begin(), actors_.end(),
			[actor](const std::unique_ptr<Actor>& candidate) { return candidate.get() == actor; });
		if (owned) return true;
		return std::any_of(pendingActorAdds_.begin(), pendingActorAdds_.end(),
			[actor](const PendingActorAdd& candidate) { return candidate.actor.get() == actor; });
	}

	void ActorWorld::ClearReferencesToActor(Actor& actor)
	{
		if (selectedActor_ == &actor) selectedActor_ = nullptr;
		if (selectedComponent_ && selectedComponent_->GetOwner() == &actor) selectedComponent_ = nullptr;
		if (pendingReloadActor_.GetWorldId() == worldId_ && pendingReloadActor_.GetId() == actor.GetId())
		{
			pendingReloadActor_.Reset();
			hasPendingReloadActor_ = false;
			pendingReloadFilePath_.clear();
		}
#ifdef USE_IMGUI
		EditorActorStateRegistry::GetInstance()->Remove(&actor);
		EditorContext::GetInstance()->GetSelection().Clear(); // EditorObjectInfoが保持する編集コールバックを削除Actorから切り離す。
#endif
	}

	void ActorWorld::FlushPendingDestroyedActors()
	{
		std::erase_if(actors_, [this](const std::unique_ptr<Actor>& actor)
			{
				if (!actor || !actor->IsPendingDestroy()) return false;
				const std::string actorName = actor->GetName();
				ClearReferencesToActor(*actor);
				UnregisterPhysicsComponents(*actor);
				actor->FinalizeForWorld();
				ReleaseActorFromWorld(*actor);
				lastActorJsonSaveMessage_ = "Destroyed : " + actorName;
				return true;
			});
	}

	std::string ActorWorld::MakeUniqueComponentName(const Actor& actor, const std::string& baseName) const
	{
		const std::string safeBaseName = baseName.empty() ? "Component" : baseName; // 空文字の場合はデフォルト名を使用する

		if (!actor.FindComponentByName(safeBaseName))
		{
			return safeBaseName; // 同名のComponentが存在しない場合はそのまま返す
		}

		for (int index = 1; index < 10000; ++index)
		{
			char nameBuffer[256]{};
			std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03d", safeBaseName.c_str(), index);

			const std::string candidateName = nameBuffer;

			if (!actor.FindComponentByName(candidateName))
			{
				return candidateName; // 同名のComponentが存在しない場合はその名前を返す
			}
		}

		return safeBaseName + "_Duplicate"; // 10000件以上同名が存在する場合は末尾に_Duplicateを付けて返す
	}

	std::string ActorWorld::MakeUniqueActorName(const std::string& baseName) const
	{
		const std::string safeBaseName = baseName.empty() ? "Actor" : baseName; // 空文字の場合はデフォルト名を使用する

		bool existsSameName = false;

		for (const auto& actor : actors_)
		{
			if (!actor)
			{
				continue; // Actorがnullptrならスキップする
			}

			if (actor->GetName() == safeBaseName)
			{
				existsSameName = true;
				break; // 同名のActorが存在する場合はループを抜ける
			}
		}
		for (const PendingActorAdd& pending : pendingActorAdds_)
		{
			if (pending.actor && pending.actor->GetName() == safeBaseName) existsSameName = true;
		}

		if (!existsSameName)
		{
			return safeBaseName; // 同名のActorが存在しない場合はそのまま返す
		}

		for (int index = 1; index < 10000; ++index)
		{
			char nameBuffer[256]{};
			std::snprintf(nameBuffer, sizeof(nameBuffer), "%s_%03d", safeBaseName.c_str(), index);

			const std::string candidateName = nameBuffer;

			bool exsits = false;

			for (const auto& actor : actors_)
			{
				if (!actor)
				{
					continue; // Actorがnullptrならスキップする
				}

				if (actor->GetName() == candidateName)
				{
					exsits = true;
					break; // 同名のActorが存在する場合はループを抜ける
				}
			}
			for (const PendingActorAdd& pending : pendingActorAdds_)
			{
				if (pending.actor && pending.actor->GetName() == candidateName)
				{
					exsits = true;
					break;
				}
			}

			if (!exsits)
			{
				return candidateName; // 同名のActorが存在しない場合はその名前を返す
			}
		}

		return safeBaseName + "_Duplicate"; // 10000件以上同名が存在する場合は末尾に_Duplicateを付けて返す
	}
}
