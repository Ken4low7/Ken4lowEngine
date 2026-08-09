#include "ActorWorld.h"

#include "ActorJsonSerializer.h"
#include "CameraComponent.h"

#include "LightManager.h"
#include "SceneComponent.h"

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#include <Editor/EditorContext.h>
#endif

#include <algorithm>
#include <cstdio>

namespace Ken4lowEngine
{
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

		RefreshActorPrefabFileList(); // Actor PrefabsのJSONファイル一覧を更新する

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

			if (!actor->IsActive())
			{
				UnregisterPhysicsComponents(*actor); // 無効なActorは物理登録から外す
				continue;
			}
			actor->InitializeComponents(); // 実行中に追加されたComponentをフレーム境界で初期化する。
			RegisterPhysicsComponents(*actor); // ComponentのActive変更を含めてPhysics登録を同期する。

			// ActorWorldは更新順だけ管理し、処理内容はActor/Component側に任せる
			actor->Update(deltaTime);
			actor->InitializeComponents(); // Update中に追加されたComponentもPhysics登録前に初期化する。
			if (actor->IsPendingDestroy() || !actor->IsActive()) UnregisterPhysicsComponents(*actor);
			else RegisterPhysicsComponents(*actor); // Update内のActive変更を直後のPhysics Stepへ反映する。

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
			if (!actor->IsActive())
			{
				UnregisterPhysicsComponents(*actor);
				continue;
			}
			actor->InitializeComponents();
			RegisterPhysicsComponents(*actor); // Edit中のActive変更も物理デバッグ登録へ反映する。
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
		isUpdating_ = false;
		selectedActor_ = nullptr;
		selectedComponent_ = nullptr;
		pendingReloadActor_ = nullptr;
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
		}

		actors_.clear(); // Finalize後にActorを破棄し、古い状態が残らないようにする
		for (PendingActorAdd& pending : pendingActorAdds_)
		{
			if (pending.actor) pending.actor->FinalizeForWorld();
		}
		pendingActorAdds_.clear();
		LightManager::GetInstance()->SetLightComponentLights({}); // ActorWorld破棄時にComponent由来ライトを解除する
#ifdef USE_IMGUI
		EditorActorStateRegistry::GetInstance()->Clear();
		EditorContext::GetInstance()->GetSelection().Clear();
#endif

		isInitialized_ = false; // 再Initialize時にSpawn済みActorを通常初期化できるように戻す
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

			cameraComponent->SetAutoRegisterMainCamera(false); // SpawnしたActorのCameraComponentをMainCameraとして登録する
		}

		AddActorToWorld(std::move(actor), false); // JSON生成済みComponentはSerializer側で初期化されている。

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
		if (!pendingReloadActor_ || !OwnsActor(pendingReloadActor_))
		{
			hasPendingReloadActor_ = false;
			pendingReloadActor_ = nullptr;
			pendingReloadFilePath_.clear();
			return; // 削除済みActorへの遅延参照は次フレームへ持ち越さない。
		}

		Actor* reloadActor = pendingReloadActor_;
		const std::string reloadFilePath = pendingReloadFilePath_;

		hasPendingReloadActor_ = false; // 次フレームで再度処理されないようにフラグを下ろす
		pendingReloadActor_ = nullptr; // 次フレームで再度処理されないようにポインタをクリアする
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
		if (pendingReloadActor_ == &actor)
		{
			pendingReloadActor_ = nullptr;
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
