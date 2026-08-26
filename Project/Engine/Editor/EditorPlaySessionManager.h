#pragma once

#include "EditorActorStateRegistry.h"
#include "EditorCommandHistory.h"
#include "EditorContext.h"
#include "EditorLevelService.h"

#include <ActorJsonSerializer.h>
#include <ActorSpawnOptions.h>
#include <ActorWorld.h>
#include <BaseScene.h>
#include <Camera.h>
#include <CameraComponent.h>
#include <CameraManager.h>
#include <DebugCamera.h>
#include <LightManager.h>
#include <SceneComponent.h>
#include <ShadowSettings.h>
#include <json.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	enum class EditorWorldDomain
	{
		Editor,
		Runtime,
	};

	/// <summary>
	/// Play開始時にEditor Worldをメモリへ退避し、別ActorインスタンスのRuntime Worldへ差し替えます。
	/// Stop時はRuntime Worldを破棄してEditor Worldを復元するため、ゲーム中の変更は編集データへ混ざりません。
	/// </summary>
	class EditorPlaySessionManager
	{
	public:
		static EditorPlaySessionManager* GetInstance()
		{
			static EditorPlaySessionManager instance;
			return &instance;
		}

		bool BeginPlaySession(BaseScene& scene)
		{
			if (sessionActive_)
			{
				SetStatus(false, "PIE Runtime Worldは既に開始されています。");
				return false;
			}

			WorldSnapshot snapshot{};
			if (!CaptureWorldSnapshot(scene, snapshot))
			{
				SetStatus(false, "Editor WorldのSnapshot作成に失敗しました。");
				return false;
			}

			editorSnapshot_ = std::move(snapshot);
			if (!ReplaceWorldFromSnapshot(scene, editorSnapshot_, EditorWorldDomain::Runtime, false))
			{
				editorSnapshot_ = {};
				SetStatus(false, lastReplaceError_.empty()
					? "PIE Runtime Worldの生成に失敗しました。Editor Worldは維持されます。"
					: lastReplaceError_);
				return false;
			}

			sessionActive_ = true;
			worldDomain_ = EditorWorldDomain::Runtime;
			runtimeElapsedSeconds_ = 0.0f;
			runtimeFrameCount_ = 0;
			runtimeActorCount_ = CountActors(scene.GetEditorActorWorld());
			SetStatus(true, "PIE Runtime WorldをEditor Worldの複製から開始しました。");
			return true;
		}

		bool EndPlaySession(BaseScene& scene, bool keepRuntimeChanges)
		{
			if (!sessionActive_ || !editorSnapshot_.valid)
			{
				SetStatus(false, "停止できるPIE Sessionがありません。");
				return false;
			}

			WorldSnapshot restoreSnapshot = editorSnapshot_;
			if (keepRuntimeChanges)
			{
				WorldSnapshot runtimeSnapshot{};
				if (!CaptureWorldSnapshot(scene, runtimeSnapshot))
				{
					SetStatus(false, "Runtime変更の取得に失敗したためEditor Worldへ反映できませんでした。");
					return false;
				}
				runtimeSnapshot.levelPath = editorSnapshot_.levelPath;
				runtimeSnapshot.levelName = editorSnapshot_.levelName;
				runtimeSnapshot.levelDirty = true;
				restoreSnapshot = std::move(runtimeSnapshot); // 明示的なKeep Changes時だけRuntime Worldを編集データへ採用する。
			}

			if (!ReplaceWorldFromSnapshot(scene, restoreSnapshot, EditorWorldDomain::Editor, true))
			{
				SetStatus(false, lastReplaceError_.empty()
					? "Editor Worldの復元に失敗しました。現在のRuntime Worldは維持されます。"
					: lastReplaceError_);
				return false;
			}

			sessionActive_ = false;
			worldDomain_ = EditorWorldDomain::Editor;
			runtimeElapsedSeconds_ = 0.0f;
			runtimeFrameCount_ = 0;
			runtimeActorCount_ = 0;
			editorSnapshot_ = {};
			SetStatus(true, keepRuntimeChanges
				? "Runtime変更をEditor Worldへ反映してPIEを停止しました。"
				: "Runtime Worldを破棄してPlay前のEditor Worldへ戻しました。");
			return true;
		}

		void CancelSessionWithoutRestore()
		{
			sessionActive_ = false;
			worldDomain_ = EditorWorldDomain::Editor;
			runtimeElapsedSeconds_ = 0.0f;
			runtimeFrameCount_ = 0;
			runtimeActorCount_ = 0;
			editorSnapshot_ = {};
			lastReplaceError_.clear();
			std::error_code error;
			std::filesystem::remove_all(kTemporaryRoot, error); // 旧PIE一時ファイルが残っていてもApplication終了時に掃除する。
		}

		void NotifyRuntimeTick(float deltaTime, const BaseScene& scene)
		{
			if (!sessionActive_) return;
			runtimeElapsedSeconds_ += (std::max)(0.0f, deltaTime);
			++runtimeFrameCount_;
			runtimeActorCount_ = CountActors(const_cast<BaseScene&>(scene).GetEditorActorWorld());
		}

		bool IsSessionActive() const { return sessionActive_; }
		bool IsRuntimeWorldActive() const { return sessionActive_ && worldDomain_ == EditorWorldDomain::Runtime; }
		EditorWorldDomain GetWorldDomain() const { return worldDomain_; }
		float GetRuntimeElapsedSeconds() const { return runtimeElapsedSeconds_; }
		uint64_t GetRuntimeFrameCount() const { return runtimeFrameCount_; }
		std::size_t GetRuntimeActorCount() const { return runtimeActorCount_; }
		const char* GetWorldDomainText() const { return IsRuntimeWorldActive() ? "PIE Runtime World" : "Editor World"; }

		bool ConsumeStatus(std::string& outMessage, bool& outSucceeded)
		{
			if (consumedStatusSerial_ == statusSerial_) return false;
			consumedStatusSerial_ = statusSerial_;
			outMessage = statusMessage_;
			outSucceeded = statusSucceeded_;
			return !outMessage.empty();
		}

	private:
		struct ActorSnapshot
		{
			std::string id;
			std::string parentId;
			nlohmann::json actorJson;
			EditorActorState editorState{};
		};

		struct CameraSnapshot
		{
			bool valid = false;
			Vector3 position{};
			Vector3 rotation{};
			Vector3 scale{ 1.0f, 1.0f, 1.0f };
			float fovY = 0.45f;
			float aspectRatio = 16.0f / 9.0f;
			float nearClip = 0.1f;
			float farClip = 1000.0f;
		};

		struct WorldSnapshot
		{
			bool valid = false;
			std::vector<ActorSnapshot> actors;
			LightManager::LightingSettingsGPU lighting{};
			ShadowSettings shadow{};
			std::vector<LightManager::PunctualLightGPU> punctualLights;
			CameraSnapshot editorCamera{};
			CameraSnapshot gameCamera{};
			std::filesystem::path levelPath;
			std::string levelName = "Untitled";
			bool levelDirty = false;
		};

		static inline const std::filesystem::path kTemporaryRoot = "../Generated/Intermediate/EditorPIE";

		EditorPlaySessionManager() = default;
		~EditorPlaySessionManager() = default;
		EditorPlaySessionManager(const EditorPlaySessionManager&) = delete;
		EditorPlaySessionManager& operator=(const EditorPlaySessionManager&) = delete;

		static std::size_t CountActors(const ActorWorld* actorWorld)
		{
			if (!actorWorld) return 0;
			return static_cast<std::size_t>(std::count_if(actorWorld->GetActors().begin(), actorWorld->GetActors().end(),
				[](const std::unique_ptr<Actor>& actor)
				{
					return actor && !actor->IsPendingDestroy();
				}));
		}

		static void FinalizeStagedActors(std::vector<std::unique_ptr<Actor>>& actors)
		{
			for (std::unique_ptr<Actor>& actor : actors)
			{
				if (actor) actor->FinalizeForWorld();
			}
			actors.clear();
		}

		static std::string DescribeSnapshotActor(const ActorSnapshot& snapshot, std::size_t index)
		{
			const std::string actorName = snapshot.actorJson.value("Name", std::string{});
			const std::string actorClass = snapshot.actorJson.value("Class", std::string{});
			std::string result = snapshot.id.empty() ? "Actor_" + std::to_string(index) : snapshot.id;
			if (!actorName.empty()) result += " / " + actorName;
			if (!actorClass.empty()) result += " [" + actorClass + "]";
			return result;
		}

		static Actor* GetParentActor(const Actor& actor)
		{
			SceneComponent* root = actor.GetRootComponent();
			SceneComponent* parentComponent = root ? root->GetParent() : nullptr;
			Actor* parentActor = parentComponent ? parentComponent->GetOwner() : nullptr;
			return parentActor != &actor ? parentActor : nullptr;
		}

		static CameraSnapshot CaptureCamera(const Camera* camera)
		{
			CameraSnapshot snapshot{};
			if (!camera) return snapshot;
			snapshot.valid = true;
			snapshot.position = camera->GetTranslate();
			snapshot.rotation = camera->GetRotate();
			snapshot.scale = camera->GetScale();
			snapshot.fovY = camera->GetFovY();
			snapshot.aspectRatio = camera->GetAspectRatio();
			snapshot.nearClip = camera->GetNearClip();
			snapshot.farClip = camera->GetFarClip();
			return snapshot;
		}

		static CameraSnapshot CaptureDebugCamera(const DebugCamera* camera)
		{
			CameraSnapshot snapshot{};
			if (!camera) return snapshot;
			snapshot.valid = true;
			snapshot.position = camera->GetTranslate();
			snapshot.rotation = camera->GetRotate();
			snapshot.scale = { 1.0f, 1.0f, 1.0f };
			snapshot.fovY = camera->GetFovY();
			snapshot.aspectRatio = camera->GetAspectRatio();
			snapshot.nearClip = camera->GetNearClip();
			snapshot.farClip = camera->GetFarClip();
			return snapshot;
		}

		static void RestoreCamera(Camera* camera, const CameraSnapshot& snapshot)
		{
			if (!camera || !snapshot.valid) return;
			camera->SetTranslate(snapshot.position);
			camera->SetRotate(snapshot.rotation);
			camera->SetScale(snapshot.scale);
			camera->SetFovY(snapshot.fovY);
			camera->SetAspectRatio(snapshot.aspectRatio);
			camera->SetNearClip(snapshot.nearClip);
			camera->SetFarClip(snapshot.farClip);
			camera->Update();
		}

		static void RestoreDebugCamera(DebugCamera* camera, const CameraSnapshot& snapshot)
		{
			if (!camera || !snapshot.valid) return;
			camera->SetTranslate(snapshot.position);
			camera->SetRotate(snapshot.rotation);
			camera->SetFovY(snapshot.fovY);
			camera->SetAspectRatio(snapshot.aspectRatio);
			camera->SetNearClip(snapshot.nearClip);
			camera->SetFarClip(snapshot.farClip);
			camera->RefreshViewProjection();
		}

		bool CaptureWorldSnapshot(BaseScene& scene, WorldSnapshot& outSnapshot)
		{
			ActorWorld* actorWorld = scene.GetEditorActorWorld();
			if (!actorWorld) return false;

			outSnapshot = {};
			std::unordered_map<const Actor*, std::string> actorIds;
			std::size_t actorIndex = 0;
			for (const auto& actorOwner : actorWorld->GetActors())
			{
				Actor* actor = actorOwner.get();
				if (!actor || actor->IsPendingDestroy()) continue;
				actorIds.emplace(actor, "Actor_" + std::to_string(actorIndex++));
			}

			outSnapshot.actors.reserve(actorIds.size());
			for (const auto& actorOwner : actorWorld->GetActors())
			{
				Actor* actor = actorOwner.get();
				if (!actor || actor->IsPendingDestroy()) continue;

				ActorSnapshot actorSnapshot{};
				actorSnapshot.id = actorIds.at(actor);
				if (Actor* parentActor = GetParentActor(*actor))
				{
					const auto parentIt = actorIds.find(parentActor);
					if (parentIt != actorIds.end()) actorSnapshot.parentId = parentIt->second;
				}
				actorSnapshot.actorJson = ActorJsonSerializer::SerializeActor(*actor);
				actorSnapshot.editorState = EditorActorStateRegistry::GetInstance()->GetState(actor);
				outSnapshot.actors.push_back(std::move(actorSnapshot));
			}

			LightManager* lightManager = LightManager::GetInstance();
			outSnapshot.lighting = lightManager->GetLightingSettings();
			outSnapshot.shadow = lightManager->GetShadowSettingsForParameter();
			outSnapshot.punctualLights = lightManager->GetPunctualLights();

			CameraManager* cameraManager = CameraManager::GetInstance();
			outSnapshot.editorCamera = CaptureDebugCamera(cameraManager->GetDebugCamera());
			outSnapshot.gameCamera = CaptureCamera(cameraManager->GetMainCamera());
			outSnapshot.levelPath = EditorLevelService::GetInstance()->GetCurrentLevelPath();
			outSnapshot.levelName = EditorContext::GetInstance()->GetActiveLevelName();
			outSnapshot.levelDirty = EditorContext::GetInstance()->IsLevelDirty();
			outSnapshot.valid = true;
			return true;
		}

		static void RestoreCameraRegistration(Actor& actor, const nlohmann::json& actorJson)
		{
			if (!actorJson.contains("Components") || !actorJson["Components"].is_array()) return;
			std::vector<CameraComponent*> cameras = actor.GetComponents<CameraComponent>();
			std::size_t cameraIndex = 0;
			for (const nlohmann::json& componentJson : actorJson["Components"])
			{
				if (!componentJson.is_object() || componentJson.value("Class", std::string{}) != "CameraComponent") continue;
				if (cameraIndex >= cameras.size()) break;
				if (cameras[cameraIndex])
				{
					cameras[cameraIndex]->SetAutoRegisterMainCamera(componentJson.value("AutoRegisterMainCamera", false));
				}
				++cameraIndex;
			}
		}

		bool ReplaceWorldFromSnapshot(BaseScene& scene, const WorldSnapshot& snapshot, EditorWorldDomain domain, bool restoreDocumentState)
		{
			lastReplaceError_.clear();
			if (!snapshot.valid)
			{
				lastReplaceError_ = "PIE World置換に使用するSnapshotが無効です。";
				return false;
			}

			ActorWorld* actorWorld = scene.GetEditorActorWorld();
			if (!actorWorld)
			{
				lastReplaceError_ = "PIE World置換対象のActorWorldを取得できませんでした。";
				return false;
			}

			std::vector<std::unique_ptr<Actor>> stagedActors;
			std::unordered_map<std::string, Actor*> stagedActorsById;
			stagedActors.reserve(snapshot.actors.size());

			ActorSpawnOptions spawnOptions{};
			spawnOptions.applySpawnOffset = false;
			spawnOptions.disableAutoRegisterMainCamera = true; // Commit前のStaging ActorからMainCameraなどのWorld状態を変更させない。

			for (std::size_t index = 0; index < snapshot.actors.size(); ++index)
			{
				const ActorSnapshot& actorSnapshot = snapshot.actors[index];
				std::unique_ptr<Actor> stagedActor = ActorJsonSerializer::CreateActorFromJson(actorSnapshot.actorJson, spawnOptions);
				if (!stagedActor)
				{
					FinalizeStagedActors(stagedActors);
					lastReplaceError_ = "PIE ActorのStagingに失敗しました。Editor Worldは維持されます: " +
						DescribeSnapshotActor(actorSnapshot, index);
					return false;
				}

				Actor* stagedActorPointer = stagedActor.get();
				stagedActorsById[actorSnapshot.id] = stagedActorPointer;
				stagedActors.push_back(std::move(stagedActor));
			}

			for (std::size_t index = 0; index < snapshot.actors.size(); ++index)
			{
				const ActorSnapshot& actorSnapshot = snapshot.actors[index];
				if (actorSnapshot.parentId.empty()) continue;

				const auto childIt = stagedActorsById.find(actorSnapshot.id);
				const auto parentIt = stagedActorsById.find(actorSnapshot.parentId);
				if (childIt == stagedActorsById.end() || parentIt == stagedActorsById.end())
				{
					FinalizeStagedActors(stagedActors);
					lastReplaceError_ = "PIE Actorの親子関係を解決できませんでした。Editor Worldは維持されます: " +
						DescribeSnapshotActor(actorSnapshot, index);
					return false;
				}

				SceneComponent* childRoot = childIt->second ? childIt->second->GetRootComponent() : nullptr;
				SceneComponent* parentRoot = parentIt->second ? parentIt->second->GetRootComponent() : nullptr;
				if (!childRoot || !parentRoot)
				{
					FinalizeStagedActors(stagedActors);
					lastReplaceError_ = "PIE Actor階層の復元に必要なRootComponentがありません。Editor Worldは維持されます: " +
						DescribeSnapshotActor(actorSnapshot, index);
					return false;
				}

				childRoot->AttachTo(parentRoot);
			}

			std::vector<Actor*> committedActors;
			if (!actorWorld->CommitStagedActors(std::move(stagedActors), &committedActors))
			{
				lastReplaceError_ = "PIE ActorのCommitを開始できませんでした。現在のWorldは維持されます。";
				return false;
			}
			if (committedActors.size() != snapshot.actors.size())
			{
				lastReplaceError_ = "PIE ActorのCommit件数がSnapshotと一致しませんでした。";
				return false;
			}

			EditorContext::GetInstance()->ResetTransientState();
			EditorCommandHistory::GetInstance()->Clear();
			actorWorld->SetSelectedEditorObject(nullptr, nullptr);

			for (std::size_t index = 0; index < committedActors.size(); ++index)
			{
				Actor* actor = committedActors[index];
				const ActorSnapshot& actorSnapshot = snapshot.actors[index];
				if (!actor)
				{
					lastReplaceError_ = "PIE ActorのCommit後ポインタが無効です: " + DescribeSnapshotActor(actorSnapshot, index);
					return false;
				}

				RestoreCameraRegistration(*actor, actorSnapshot.actorJson);
				EditorActorStateRegistry::GetInstance()->SetState(actor, actorSnapshot.editorState);
			}

			LightManager* lightManager = LightManager::GetInstance();
			lightManager->GetMutableLightingSettingsForEditor() = snapshot.lighting;
			lightManager->SetShadowSettingsFromParameter(snapshot.shadow);
			lightManager->GetMutablePunctualLightsForEditor() = snapshot.punctualLights;

			CameraManager* cameraManager = CameraManager::GetInstance();
			RestoreDebugCamera(cameraManager->GetDebugCamera(), snapshot.editorCamera);
			RestoreCamera(cameraManager->GetMainCamera(), snapshot.gameCamera);

			actorWorld->SetSelectedEditorObject(nullptr, nullptr);
			EditorContext::GetInstance()->GetSelection().Clear();
			if (restoreDocumentState)
			{
				EditorContext::GetInstance()->SetActiveLevelName(snapshot.levelName);
				EditorContext::GetInstance()->MarkLevelDirty(snapshot.levelDirty);
			}
			else
			{
				EditorContext::GetInstance()->SetActiveLevelName(snapshot.levelName + " [PIE]");
				EditorContext::GetInstance()->MarkLevelDirty(false);
			}

			std::error_code error;
			std::filesystem::remove_all(kTemporaryRoot, error);
			worldDomain_ = domain;
			return true;
		}

		void SetStatus(bool succeeded, std::string message)
		{
			statusSucceeded_ = succeeded;
			statusMessage_ = std::move(message);
			++statusSerial_;
		}

		WorldSnapshot editorSnapshot_{};
		bool sessionActive_ = false;
		EditorWorldDomain worldDomain_ = EditorWorldDomain::Editor;
		float runtimeElapsedSeconds_ = 0.0f;
		uint64_t runtimeFrameCount_ = 0;
		std::size_t runtimeActorCount_ = 0;
		std::string statusMessage_;
		std::string lastReplaceError_;
		bool statusSucceeded_ = true;
		uint64_t statusSerial_ = 0;
		uint64_t consumedStatusSerial_ = 0;
	};
} // namespace Ken4lowEngine
