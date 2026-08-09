#define NOMINMAX
#include "ActorWorldEditorBridge.h"

#include "ActorJsonSerializer.h"
#include "EditorActorStateRegistry.h"
#include "EditorCommandHistory.h"

#include <CameraManager.h>
#include <DebugCamera.h>
#include <SceneComponent.h>
#include <json.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		uint64_t MakeActorEditorId(std::string_view sceneName, const Actor* actor)
		{
			const std::string actorKey = std::string(sceneName) + "/Actor/" +
				std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(actor)));
			return MakeStableEditorObjectId(actorKey);
		}

		uint64_t MakeFolderEditorId(std::string_view sceneName, std::string_view folderPath)
		{
			return MakeStableEditorObjectId(std::string(sceneName) + "/Folder/" + std::string(folderPath));
		}

		std::string NormalizeFolderPath(std::string_view path)
		{
			std::string normalized(path);
			std::replace(normalized.begin(), normalized.end(), '\\', '/');
			while (!normalized.empty() && normalized.front() == '/') normalized.erase(normalized.begin());
			while (!normalized.empty() && normalized.back() == '/') normalized.pop_back();

			std::string compact;
			compact.reserve(normalized.size());
			for (const char character : normalized)
			{
				if (character == '/' && !compact.empty() && compact.back() == '/') continue;
				compact.push_back(character);
			}
			return compact;
		}

		std::string GetFolderName(std::string_view folderPath)
		{
			const size_t separator = folderPath.find_last_of('/');
			return separator == std::string_view::npos
				? std::string(folderPath)
				: std::string(folderPath.substr(separator + 1));
		}

		std::string GetParentFolderPath(std::string_view folderPath)
		{
			const size_t separator = folderPath.find_last_of('/');
			return separator == std::string_view::npos ? std::string{} : std::string(folderPath.substr(0, separator));
		}

		void CollectFolderPrefixes(std::string_view folderPath, std::unordered_set<std::string>& outFolders)
		{
			const std::string normalized = NormalizeFolderPath(folderPath);
			if (normalized.empty()) return;

			size_t cursor = 0;
			while (cursor < normalized.size())
			{
				const size_t separator = normalized.find('/', cursor);
				outFolders.insert(normalized.substr(0, separator));
				if (separator == std::string::npos) break;
				cursor = separator + 1;
			}
		}

		Actor* GetParentActor(const Actor* actor)
		{
			if (!actor || !actor->GetRootComponent()) return nullptr;
			SceneComponent* parentComponent = actor->GetRootComponent()->GetParent();
			Actor* parentActor = parentComponent ? parentComponent->GetOwner() : nullptr;
			return parentActor != actor ? parentActor : nullptr;
		}

		Actor* FindActorByEditorId(ActorWorld& actorWorld, std::string_view sceneName, uint64_t objectId)
		{
			for (const auto& actorOwner : actorWorld.GetActors())
			{
				Actor* actor = actorOwner.get();
				if (actor && !actor->IsPendingDestroy() && MakeActorEditorId(sceneName, actor) == objectId) return actor;
			}
			return nullptr;
		}

		bool WouldCreateActorCycle(Actor* actor, Actor* proposedParent)
		{
			for (Actor* current = proposedParent; current; current = GetParentActor(current))
			{
				if (current == actor) return true;
			}
			return false;
		}

		EditorTransform ReadSceneComponentWorldTransform(const SceneComponent& component)
		{
			EditorTransform transform{};
			transform.position = component.GetWorldPosition();
			transform.rotation = component.GetWorldRotation();
			transform.scale = component.GetWorldScale();
			return transform;
		}

		void WriteSceneComponentWorldTransform(SceneComponent& component, const EditorTransform& worldTransform)
		{
			EditorTransform localTransform = worldTransform;
			if (const SceneComponent* parent = component.GetParent())
			{
				const Vector3& parentPosition = parent->GetWorldPosition();
				const Vector3& parentRotation = parent->GetWorldRotation();
				const Vector3& parentScale = parent->GetWorldScale();
				localTransform.position = worldTransform.position - parentPosition;
				localTransform.rotation = worldTransform.rotation - parentRotation;
				localTransform.scale = {
					std::abs(parentScale.x) > 0.0001f ? worldTransform.scale.x / parentScale.x : worldTransform.scale.x,
					std::abs(parentScale.y) > 0.0001f ? worldTransform.scale.y / parentScale.y : worldTransform.scale.y,
					std::abs(parentScale.z) > 0.0001f ? worldTransform.scale.z / parentScale.z : worldTransform.scale.z,
				};
			}
			component.SetLocalPosition(localTransform.position);
			component.SetLocalRotation(localTransform.rotation);
			component.SetLocalScale(localTransform.scale);
			component.RefreshWorldTransform();
		}

		void ApplyActorParent(Actor* actor, Actor* parentActor, const EditorTransform& worldTransform)
		{
			if (!actor || !actor->GetRootComponent()) return;
			SceneComponent* root = actor->GetRootComponent();
			root->Detach();
			if (parentActor && parentActor->GetRootComponent()) root->AttachTo(parentActor->GetRootComponent());
			WriteSceneComponentWorldTransform(*root, worldTransform); // 親変更前のWorld Transformを維持する。
		}

		void FocusEditorCamera(const Vector3& target)
		{
			CameraManager* cameraManager = CameraManager::GetInstance();
			DebugCamera* debugCamera = cameraManager ? cameraManager->GetDebugCamera() : nullptr;
			if (!debugCamera) return;
			const Vector3 forward = Vector3::NormalizeSafe(cameraManager->GetActiveCameraForward(), { 0.0f, 0.0f, 1.0f });
			debugCamera->SetTranslate(target - forward * 8.0f);
			debugCamera->RefreshViewProjection();
		}

		void ExecuteActiveCommand(const EditorObjectInfo& object, bool active)
		{
			bool before = active;
			if (!object.ReadActive(before) || before == active) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<bool>>(
				"有効状態変更", before, active,
				[object](const bool& value)
				{
					object.WriteActive(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		void ExecuteVisibilityCommand(const EditorObjectInfo& object, bool visible)
		{
			bool before = visible;
			if (!object.ReadVisible(before) || before == visible) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<bool>>(
				"Editor表示変更", before, visible,
				[object](const bool& value)
				{
					object.WriteVisible(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		void ExecuteLockedCommand(const EditorObjectInfo& object, bool locked)
		{
			bool before = locked;
			if (!object.ReadLocked(before) || before == locked) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<bool>>(
				"Editorロック変更", before, locked,
				[object](const bool& value)
				{
					object.WriteLocked(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		void ExecuteRenameCommand(const EditorObjectInfo& object, std::string_view name)
		{
			if (name.empty() || object.displayName == name) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<std::string>>(
				"名前変更", object.displayName, std::string(name),
				[object](const std::string& value)
				{
					object.Rename(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		void ExecuteFolderCommand(const EditorObjectInfo& object, std::string_view folderPath)
		{
			const std::string normalized = NormalizeFolderPath(folderPath);
			if (object.folderPath == normalized) return;
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorValueCommand<std::string>>(
				"フォルダー変更", object.folderPath, normalized,
				[object](const std::string& value)
				{
					object.SetFolder(value);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		void ExecuteReparentCommand(ActorWorld& actorWorld, Actor* actor, Actor* newParent)
		{
			if (!actor || !actor->GetRootComponent() || actor == newParent || WouldCreateActorCycle(actor, newParent)) return;
			Actor* oldParent = GetParentActor(actor);
			if (oldParent == newParent) return;
			const EditorTransform worldTransform = ReadSceneComponentWorldTransform(*actor->GetRootComponent());

			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorLambdaCommand>(
				"アクタ親子変更",
				[&actorWorld, actor, newParent, worldTransform]()
				{
					ApplyActorParent(actor, newParent, worldTransform);
					actorWorld.SetSelectedEditorObject(actor, nullptr);
					EditorContext::GetInstance()->MarkLevelDirty();
				},
				[&actorWorld, actor, oldParent, worldTransform]()
				{
					ApplyActorParent(actor, oldParent, worldTransform);
					actorWorld.SetSelectedEditorObject(actor, nullptr);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		std::string MakeEditorActorSnapshotPath(const char* prefix, const Actor* actor)
		{
			static std::atomic_uint64_t serial = 0;
			return "../Generated/Intermediate/EditorUndo/" + std::string(prefix) + "_" +
				std::to_string(reinterpret_cast<uintptr_t>(actor)) + "_" + std::to_string(++serial) + ".json";
		}

		void SelectActor(ActorWorld& actorWorld, Actor* actor)
		{
			actorWorld.SetSelectedEditorObject(actor, nullptr);
			EditorContext::GetInstance()->GetSelection().Clear();
		}

		void ExecuteDuplicateActorCommand(ActorWorld& actorWorld, Actor* sourceActor)
		{
			if (!sourceActor) return;
			const std::string snapshotPath = MakeEditorActorSnapshotPath("DuplicateActor", sourceActor);
			if (!ActorJsonSerializer::SaveActorToFile(*sourceActor, snapshotPath)) return;

			const EditorActorState sourceState = EditorActorStateRegistry::GetInstance()->GetState(sourceActor);
			Actor* sourceParent = GetParentActor(sourceActor);
			const EditorTransform sourceWorld = sourceActor->GetRootComponent()
				? ReadSceneComponentWorldTransform(*sourceActor->GetRootComponent())
				: EditorTransform{};
			auto duplicatedActor = std::make_shared<Actor*>(nullptr);

			EditorCommandHistory::GetInstance()->Clear();
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorLambdaCommand>(
				"アクタ複製",
				[&actorWorld, duplicatedActor, snapshotPath, sourceState, sourceParent, sourceWorld]()
				{
					*duplicatedActor = actorWorld.SpawnActorFromJson(snapshotPath);
					if (!*duplicatedActor) return;
					EditorActorStateRegistry::GetInstance()->SetState(*duplicatedActor, sourceState);
					if ((*duplicatedActor)->GetRootComponent()) ApplyActorParent(*duplicatedActor, sourceParent, sourceWorld);
					SelectActor(actorWorld, *duplicatedActor);
					EditorContext::GetInstance()->MarkLevelDirty();
				},
				[&actorWorld, duplicatedActor]()
				{
					if (!*duplicatedActor) return;
					EditorActorStateRegistry::GetInstance()->Remove(*duplicatedActor);
					(*duplicatedActor)->Destroy();
					*duplicatedActor = nullptr;
					SelectActor(actorWorld, nullptr);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}

		struct ChildActorState
		{
			Actor* actor = nullptr;
			EditorTransform worldTransform{};
		};

		void ExecuteDeleteActorCommand(ActorWorld& actorWorld, Actor* sourceActor)
		{
			if (!sourceActor) return;
			const std::string snapshotPath = MakeEditorActorSnapshotPath("DeleteActor", sourceActor);
			if (!ActorJsonSerializer::SaveActorToFile(*sourceActor, snapshotPath)) return;

			const EditorActorState sourceState = EditorActorStateRegistry::GetInstance()->GetState(sourceActor);
			Actor* sourceParent = GetParentActor(sourceActor);
			const EditorTransform sourceWorld = sourceActor->GetRootComponent()
				? ReadSceneComponentWorldTransform(*sourceActor->GetRootComponent())
				: EditorTransform{};
			std::vector<ChildActorState> childActors;
			for (const auto& actorOwner : actorWorld.GetActors())
			{
				Actor* child = actorOwner.get();
				if (child && GetParentActor(child) == sourceActor && child->GetRootComponent())
				{
					childActors.push_back({ child, ReadSceneComponentWorldTransform(*child->GetRootComponent()) });
				}
			}
			auto currentActor = std::make_shared<Actor*>(sourceActor);

			EditorCommandHistory::GetInstance()->Clear();
			EditorCommandHistory::GetInstance()->Execute(std::make_unique<EditorLambdaCommand>(
				"アクタ削除",
				[&actorWorld, currentActor, childActors]()
				{
					if (!*currentActor) return;
					for (const ChildActorState& childState : childActors)
					{
						ApplyActorParent(childState.actor, nullptr, childState.worldTransform);
					}
					EditorActorStateRegistry::GetInstance()->Remove(*currentActor);
					(*currentActor)->Destroy();
					*currentActor = nullptr;
					SelectActor(actorWorld, nullptr);
					EditorContext::GetInstance()->MarkLevelDirty();
				},
				[&actorWorld, currentActor, snapshotPath, sourceState, sourceParent, sourceWorld, childActors]()
				{
					*currentActor = actorWorld.SpawnActorFromJson(snapshotPath);
					if (!*currentActor) return;
					EditorActorStateRegistry::GetInstance()->SetState(*currentActor, sourceState);
					if ((*currentActor)->GetRootComponent()) ApplyActorParent(*currentActor, sourceParent, sourceWorld);
					for (const ChildActorState& childState : childActors)
					{
						ApplyActorParent(childState.actor, *currentActor, childState.worldTransform);
					}
					SelectActor(actorWorld, *currentActor);
					EditorContext::GetInstance()->MarkLevelDirty();
				}));
		}
	}

	bool BuildInstancedModelInstanceEditorInfo(InstancedModelComponent* component, size_t instanceIndex, uint64_t componentId, std::string_view sceneName, EditorObjectInfo& outInfo)
	{
		if (!component || instanceIndex >= component->GetEditableInstanceCount()) return false;
		Actor* owner = component->GetOwner();
		const bool locked = owner && EditorActorStateRegistry::GetInstance()->IsLocked(owner);

		const std::string instanceKey = std::to_string(componentId) + "/Instance/" + std::to_string(instanceIndex);
		outInfo = {};
		outInfo.id = MakeStableEditorObjectId(instanceKey);
		outInfo.parentId = componentId;
		outInfo.sortOrder = static_cast<int>(std::min<size_t>(instanceIndex, static_cast<size_t>(std::numeric_limits<int>::max())));
		outInfo.displayName = "Instance " + std::to_string(instanceIndex);
		outInfo.typeName = "InstancedModelInstance";
		outInfo.sceneName = std::string(sceneName);
		outInfo.icon = "[I]";
		outInfo.objectKind = EditorObjectKind::Instance;
		outInfo.canEditTransform = !locked;
		outInfo.transformUnavailableReason = locked ? "所有Actorがロックされています。" : outInfo.transformUnavailableReason;
		outInfo.inspectorType = EditorInspectorType::Transform;
		outInfo.inspectorHint = "GPU Instancing内の個別Transform";
		outInfo.readTransform = [component, instanceIndex](EditorTransform& outTransform)
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, transform)) return false;
				outTransform.position = transform.position;
				outTransform.rotation = transform.rotation;
				outTransform.scale = transform.scale;
				return true;
			};
		outInfo.writeTransform = [component, instanceIndex](const EditorTransform& transform)
			{
				InstancedModelComponent::InstanceTransform instanceTransform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, instanceTransform)) return;
				instanceTransform.position = transform.position;
				instanceTransform.rotation = transform.rotation;
				instanceTransform.scale = transform.scale;
				component->SetInstanceLocalTransform(instanceIndex, instanceTransform);
			};
		outInfo.readWorldTransform = [component, instanceIndex](EditorTransform& outTransform)
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceWorldTransform(instanceIndex, transform)) return false;
				outTransform.position = transform.position;
				outTransform.rotation = transform.rotation;
				outTransform.scale = transform.scale;
				return true;
			};
		outInfo.writeWorldTransform = [component, instanceIndex](const EditorTransform& transform)
			{
				InstancedModelComponent::InstanceTransform instanceTransform{};
				if (!component->GetInstanceWorldTransform(instanceIndex, instanceTransform)) return;
				instanceTransform.position = transform.position;
				instanceTransform.rotation = transform.rotation;
				instanceTransform.scale = transform.scale;
				component->SetInstanceWorldTransform(instanceIndex, instanceTransform);
			};
		outInfo.canFocus = true;
		outInfo.requestFocus = [component, instanceIndex]()
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (component->GetInstanceWorldTransform(instanceIndex, transform)) FocusEditorCamera(transform.position);
			};
		outInfo.canCaptureState = !locked;
		outInfo.captureState = [component, instanceIndex]()
			{
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, transform)) return std::string{};
				nlohmann::json state = {
					{ "Position", { transform.position.x, transform.position.y, transform.position.z } },
					{ "Rotation", { transform.rotation.x, transform.rotation.y, transform.rotation.z } },
					{ "Scale", { transform.scale.x, transform.scale.y, transform.scale.z } },
				};
				return state.dump();
			};
		outInfo.restoreState = [component, instanceIndex](std::string_view stateText)
			{
				const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
				if (state.is_discarded()) return;
				InstancedModelComponent::InstanceTransform transform{};
				if (!component->GetInstanceLocalTransform(instanceIndex, transform)) return;
				const auto position = state.value("Position", std::vector<float>{});
				const auto rotation = state.value("Rotation", std::vector<float>{});
				const auto scale = state.value("Scale", std::vector<float>{});
				if (position.size() == 3) transform.position = { position[0], position[1], position[2] };
				if (rotation.size() == 3) transform.rotation = { rotation[0], rotation[1], rotation[2] };
				if (scale.size() == 3) transform.scale = { scale[0], scale[1], scale[2] };
				component->SetInstanceLocalTransform(instanceIndex, transform);
			};
		outInfo.canDrawObjectId = false;
		return true;
	}

	void CollectActorWorldEditorObjects(ActorWorld& actorWorld, std::vector<EditorObjectInfo>& outObjects, std::string_view sceneName)
	{
		const std::string sceneNameText(sceneName);
		EditorActorStateRegistry* stateRegistry = EditorActorStateRegistry::GetInstance();
		std::unordered_map<const Actor*, uint64_t> actorIds;
		std::unordered_set<std::string> folderPaths;

		for (const auto& actorOwner : actorWorld.GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || actor->IsPendingDestroy()) continue;
			actorIds.emplace(actor, MakeActorEditorId(sceneNameText, actor));
			CollectFolderPrefixes(stateRegistry->GetFolderPath(actor), folderPaths);
		}

		std::vector<std::string> sortedFolders(folderPaths.begin(), folderPaths.end());
		std::sort(sortedFolders.begin(), sortedFolders.end());
		for (size_t folderIndex = 0; folderIndex < sortedFolders.size(); ++folderIndex)
		{
			const std::string& folderPath = sortedFolders[folderIndex];
			EditorObjectInfo folderInfo{};
			folderInfo.id = MakeFolderEditorId(sceneNameText, folderPath);
			const std::string parentFolder = GetParentFolderPath(folderPath);
			folderInfo.parentId = parentFolder.empty() ? 0 : MakeFolderEditorId(sceneNameText, parentFolder);
			folderInfo.sortOrder = static_cast<int>(folderIndex);
			folderInfo.displayName = GetFolderName(folderPath);
			folderInfo.typeName = "Folder";
			folderInfo.sceneName = sceneNameText;
			folderInfo.icon = "[F]";
			folderInfo.folderPath = folderPath;
			folderInfo.objectKind = EditorObjectKind::Folder;
			folderInfo.inspectorHint = folderPath;
			outObjects.push_back(std::move(folderInfo));
		}

		int actorSortOrder = 0;
		for (const auto& actorOwner : actorWorld.GetActors())
		{
			Actor* actor = actorOwner.get();
			if (!actor || actor->IsPendingDestroy()) continue;
			const uint64_t actorId = actorIds.at(actor);
			const EditorActorState actorState = stateRegistry->GetState(actor);
			Actor* parentActor = GetParentActor(actor);

			EditorObjectInfo actorInfo{};
			actorInfo.id = actorId;
			actorInfo.parentId = parentActor && actorIds.contains(parentActor)
				? actorIds.at(parentActor)
				: (actorState.folderPath.empty() ? 0 : MakeFolderEditorId(sceneNameText, NormalizeFolderPath(actorState.folderPath)));
			actorInfo.sortOrder = actorSortOrder++;
			actorInfo.displayName = actor->GetName().empty() ? actor->GetClassTypeName() : actor->GetName();
			actorInfo.typeName = actor->GetClassTypeName();
			actorInfo.sceneName = sceneNameText;
			actorInfo.icon = "[A]";
			actorInfo.folderPath = NormalizeFolderPath(actorState.folderPath);
			actorInfo.objectKind = EditorObjectKind::Actor;
			actorInfo.canToggleActive = true;
			actorInfo.readActive = [actor]() { return actor->IsActive(); };
			actorInfo.writeActive = [actor](bool active) { actor->SetActive(active); };
			actorInfo.canToggleVisibility = true;
			actorInfo.readVisible = [actor]() { return EditorActorStateRegistry::GetInstance()->IsVisible(actor); };
			actorInfo.writeVisible = [actor](bool visible) { EditorActorStateRegistry::GetInstance()->SetVisible(actor, visible); };
			actorInfo.canToggleLocked = true;
			actorInfo.readLocked = [actor]() { return EditorActorStateRegistry::GetInstance()->IsLocked(actor); };
			actorInfo.writeLocked = [actor](bool locked) { EditorActorStateRegistry::GetInstance()->SetLocked(actor, locked); };
			actorInfo.canRename = true;
			actorInfo.rename = [actor](std::string_view name) { actor->SetName(name); };
			actorInfo.canDuplicate = true;
			actorInfo.requestDuplicate = [&actorWorld, actor]() { ExecuteDuplicateActorCommand(actorWorld, actor); };
			actorInfo.canDelete = true;
			actorInfo.requestDelete = [&actorWorld, actor]() { ExecuteDeleteActorCommand(actorWorld, actor); };
			actorInfo.canFocus = actor->GetRootComponent() != nullptr;
			actorInfo.requestFocus = [actor]()
				{
					if (actor->GetRootComponent()) FocusEditorCamera(actor->GetRootComponent()->GetWorldPosition());
				};
			actorInfo.canReparent = actor->GetRootComponent() != nullptr;
			actorInfo.requestReparent = [&actorWorld, actor, sceneNameText](uint64_t targetParentId)
				{
					Actor* targetParent = targetParentId == 0 ? nullptr : FindActorByEditorId(actorWorld, sceneNameText, targetParentId);
					ExecuteReparentCommand(actorWorld, actor, targetParent);
				};
			actorInfo.canSetFolder = true;
			actorInfo.setFolder = [actor](std::string_view folderPath)
				{
					EditorActorStateRegistry::GetInstance()->SetFolderPath(actor, NormalizeFolderPath(folderPath));
				};
			actorInfo.inspectorHint = actorState.locked ? "ActorはEditorでロックされています。" : "Actor / Component Details";
			actorInfo.canCaptureState = true;
			actorInfo.captureState = [actor]()
				{
					nlohmann::json state;
					actor->ToJson(state);
					state.erase("Components");
					return state.dump();
				};
			actorInfo.restoreState = [actor](std::string_view stateText)
				{
					const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
					if (!state.is_discarded()) actor->FromJson(state);
				};

			if (SceneComponent* root = actor->GetRootComponent())
			{
				actorInfo.canEditTransform = !actorState.locked;
				actorInfo.transformUnavailableReason = actorState.locked ? "Actorがロックされています。" : actorInfo.transformUnavailableReason;
				actorInfo.inspectorType = EditorInspectorType::Transform;
				actorInfo.readTransform = [root](EditorTransform& outTransform)
					{
						outTransform.position = root->GetLocalPosition();
						outTransform.rotation = root->GetLocalRotation();
						outTransform.scale = root->GetLocalScale();
						return true;
					};
				actorInfo.writeTransform = [root](const EditorTransform& transform)
					{
						root->SetLocalPosition(transform.position);
						root->SetLocalRotation(transform.rotation);
						root->SetLocalScale(transform.scale);
						root->RefreshWorldTransform();
					};
				actorInfo.readWorldTransform = [root](EditorTransform& outTransform)
					{
						outTransform = ReadSceneComponentWorldTransform(*root);
						return true;
					};
				actorInfo.writeWorldTransform = [root](const EditorTransform& transform) { WriteSceneComponentWorldTransform(*root, transform); };
			}

			actorInfo.drawInspector = [&actorWorld, actor]()
				{
					actorWorld.SetSelectedEditorObject(actor, nullptr);
					actorWorld.DrawSelectedInspectorContent();
				};

			EditorObjectInfo actorCommandProxy = actorInfo;
			actorInfo.writeActive = [actorCommandProxy](bool active) { ExecuteActiveCommand(actorCommandProxy, active); };
			actorInfo.writeVisible = [actorCommandProxy](bool visible) { ExecuteVisibilityCommand(actorCommandProxy, visible); };
			actorInfo.writeLocked = [actorCommandProxy](bool locked) { ExecuteLockedCommand(actorCommandProxy, locked); };
			actorInfo.rename = [actorCommandProxy](std::string_view name) { ExecuteRenameCommand(actorCommandProxy, name); };
			actorInfo.setFolder = [actorCommandProxy](std::string_view folderPath) { ExecuteFolderCommand(actorCommandProxy, folderPath); };
			outObjects.push_back(std::move(actorInfo));

			std::unordered_map<const ActorComponent*, uint64_t> componentIds;
			int componentSortOrder = 0;
			const std::string actorKey = sceneNameText + "/Actor/" +
				std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(actor)));
			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (!component) continue;
				const std::string componentKey = actorKey + "/Component/" +
					std::to_string(static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(component)));
				componentIds.emplace(component, MakeStableEditorObjectId(componentKey));
			}

			for (const auto& componentOwner : actor->GetComponents())
			{
				ActorComponent* component = componentOwner.get();
				if (!component) continue;

				EditorObjectInfo componentInfo{};
				componentInfo.id = componentIds.at(component);
				componentInfo.parentId = actorId;
				componentInfo.sortOrder = componentSortOrder++;
				componentInfo.displayName = component->GetName().empty() ? component->GetClassTypeName() : component->GetName();
				componentInfo.typeName = component->GetClassTypeName();
				componentInfo.sceneName = sceneNameText;
				componentInfo.icon = "[C]";
				componentInfo.objectKind = EditorObjectKind::Component;
				componentInfo.isRootComponent = component == actor->GetRootComponent();
				componentInfo.canToggleActive = true;
				componentInfo.readActive = [component]() { return component->IsActive(); };
				componentInfo.writeActive = [component](bool active) { component->SetActive(active); };
				componentInfo.canRename = true;
				componentInfo.rename = [component](std::string_view name) { component->SetName(name); };
				componentInfo.canFocus = dynamic_cast<SceneComponent*>(component) != nullptr;
				componentInfo.requestFocus = [component]()
					{
						if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(component)) FocusEditorCamera(sceneComponent->GetWorldPosition());
					};
				componentInfo.inspectorHint = componentInfo.isRootComponent ? "Root Component" : "Actor Component";
				componentInfo.canCaptureState = !actorState.locked;
				componentInfo.captureState = [component]()
					{
						nlohmann::json state;
						component->ToJson(state);
						return state.dump();
					};
				componentInfo.restoreState = [component](std::string_view stateText)
					{
						const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
						if (!state.is_discarded()) component->FromJson(state);
					};
				componentInfo.canDrawObjectId = actor->IsActive() && actorState.visible && !actorState.locked &&
					component->IsActiveInHierarchy() && component->SupportsEditorObjectId();
				componentInfo.drawObjectId = [component](uint32_t objectId) { component->DrawEditorObjectId(objectId); };

				if (auto* sceneComponent = dynamic_cast<SceneComponent*>(component))
				{
					if (SceneComponent* parent = sceneComponent->GetParent())
					{
						const auto parentId = componentIds.find(parent);
						if (parentId != componentIds.end()) componentInfo.parentId = parentId->second;
					}
					componentInfo.canEditTransform = !actorState.locked;
					componentInfo.transformUnavailableReason = actorState.locked ? "所有Actorがロックされています。" : componentInfo.transformUnavailableReason;
					componentInfo.inspectorType = EditorInspectorType::Transform;
					componentInfo.readTransform = [sceneComponent](EditorTransform& outTransform)
						{
							outTransform.position = sceneComponent->GetLocalPosition();
							outTransform.rotation = sceneComponent->GetLocalRotation();
							outTransform.scale = sceneComponent->GetLocalScale();
							return true;
						};
					componentInfo.writeTransform = [sceneComponent](const EditorTransform& transform)
						{
							sceneComponent->SetLocalPosition(transform.position);
							sceneComponent->SetLocalRotation(transform.rotation);
							sceneComponent->SetLocalScale(transform.scale);
							sceneComponent->RefreshWorldTransform();
						};
					componentInfo.readWorldTransform = [sceneComponent](EditorTransform& outTransform)
						{
							outTransform = ReadSceneComponentWorldTransform(*sceneComponent);
							return true;
						};
					componentInfo.writeWorldTransform = [sceneComponent](const EditorTransform& transform) { WriteSceneComponentWorldTransform(*sceneComponent, transform); };
				}

				componentInfo.drawInspector = [&actorWorld, actor, component]()
					{
						actorWorld.SetSelectedEditorObject(actor, component);
						actorWorld.DrawSelectedInspectorContent();
					};

				EditorObjectInfo componentCommandProxy = componentInfo;
				componentInfo.writeActive = [componentCommandProxy](bool active) { ExecuteActiveCommand(componentCommandProxy, active); };
				componentInfo.rename = [componentCommandProxy](std::string_view name) { ExecuteRenameCommand(componentCommandProxy, name); };

				InstancedModelComponent* instancedComponent = dynamic_cast<InstancedModelComponent*>(component);
				if (instancedComponent && componentInfo.canDrawObjectId)
				{
					const size_t instanceCount = instancedComponent->GetEditableInstanceCount();
					componentInfo.objectIdSpan = static_cast<uint32_t>(std::min<size_t>(instanceCount, std::numeric_limits<uint32_t>::max()));
					const uint64_t componentId = componentInfo.id;
					componentInfo.buildObjectIdEntry = [instancedComponent, componentId, sceneNameText](uint32_t offset, EditorObjectInfo& outInfo)
						{
							return BuildInstancedModelInstanceEditorInfo(instancedComponent, offset, componentId, sceneNameText, outInfo);
						};
				}

				const uint64_t componentId = componentInfo.id;
				outObjects.push_back(std::move(componentInfo));
				if (!instancedComponent) continue;

				constexpr size_t kOutlinerInstanceLimit = 512;
				const size_t instanceCount = instancedComponent->GetEditableInstanceCount();
				const size_t visibleCount = std::min(instanceCount, kOutlinerInstanceLimit);
				for (size_t instanceIndex = 0; instanceIndex < visibleCount; ++instanceIndex)
				{
					EditorObjectInfo instanceInfo{};
					if (BuildInstancedModelInstanceEditorInfo(instancedComponent, instanceIndex, componentId, sceneNameText, instanceInfo)) outObjects.push_back(std::move(instanceInfo));
				}

				const EditorSelection& selection = EditorContext::GetInstance()->GetSelection();
				if (instanceCount > visibleCount && selection.HasSelection())
				{
					const EditorObjectInfo& selected = selection.GetSelected();
					if (selected.objectKind == EditorObjectKind::Instance && selected.parentId == componentId && selected.sortOrder >= 0)
					{
						const size_t selectedIndex = static_cast<size_t>(selected.sortOrder);
						if (selectedIndex >= visibleCount && selectedIndex < instanceCount)
						{
							EditorObjectInfo selectedInfo{};
							if (BuildInstancedModelInstanceEditorInfo(instancedComponent, selectedIndex, componentId, sceneNameText, selectedInfo)) outObjects.push_back(std::move(selectedInfo));
						}
					}
				}
			}
		}
	}
} // namespace Ken4lowEngine
