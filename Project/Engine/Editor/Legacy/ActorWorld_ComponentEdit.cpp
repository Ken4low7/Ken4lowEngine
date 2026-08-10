#include "ActorWorld.h"

#include "ActorJsonSerializer.h"
#include "ComponentFactory.h"
#include "SceneComponent.h"
#include <Editor/EditorCommandHistory.h>
#include <Editor/EditorContext.h>

#include <json.hpp>
#include <memory>
#include <string>

namespace Ken4lowEngine
{
	void ActorWorld::AddComponentToSelectedActor(std::string_view componentClassName)
	{
		Actor* targetActor = selectedActor_;
		if (!targetActor && selectedComponent_) targetActor = selectedComponent_->GetOwner();
		if (!targetActor)
		{
			lastActorJsonSaveMessage_ = "Add Component failed : no selected Actor";
			return;
		}

		const std::string className{ componentClassName };
		if (!ComponentFactory::IsAllowMultiple(className) && targetActor->HasComponentClass(className))
		{
			lastActorJsonSaveMessage_ = "Add Component failed : already exists " + className;
			return;
		}

		const std::string beforeState = ActorJsonSerializer::SerializeActor(*targetActor).dump();
		const bool wasPhysicsRegistered = targetActor->IsPhysicsRegistered();
		if (wasPhysicsRegistered) UnregisterPhysicsComponents(*targetActor);

		ActorComponent* newComponent = nullptr;
		if (!targetActor->GetRootComponent()) newComponent = ComponentFactory::CreateRootSceneComponent(targetActor, componentClassName);
		if (!newComponent) newComponent = ComponentFactory::CreateComponent(targetActor, componentClassName);
		if (!newComponent)
		{
			if (wasPhysicsRegistered) RegisterPhysicsComponents(*targetActor);
			lastActorJsonSaveMessage_ = "Add Component failed : " + className;
			return;
		}

		newComponent->SetName(MakeUniqueComponentName(*targetActor, className));
		if (SceneComponent* newSceneComponent = dynamic_cast<SceneComponent*>(newComponent))
		{
			SceneComponent* rootComponent = targetActor->GetRootComponent();
			if (rootComponent && rootComponent != newSceneComponent) newSceneComponent->AttachTo(rootComponent);
			newSceneComponent->RefreshWorldTransform();
		}
		if (isInitialized_) newComponent->InitializeForWorld();
		if (wasPhysicsRegistered) RegisterPhysicsComponents(*targetActor);

		selectedActor_ = nullptr;
		selectedComponent_ = newComponent;
		requestFocusActorDetails_ = true;
		lastActorJsonSaveMessage_ = "Added Component : " + newComponent->GetName();

		const std::string afterState = ActorJsonSerializer::SerializeActor(*targetActor).dump();
		auto applyState = [this, targetActor](const std::string& stateText)
			{
				const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
				if (state.is_discarded()) return;
				UnregisterPhysicsComponents(*targetActor);
				ActorJsonSerializer::LoadActorFromJson(*targetActor, state);
				RegisterPhysicsComponents(*targetActor);
				selectedActor_ = targetActor;
				selectedComponent_ = nullptr;
				EditorContext::GetInstance()->GetSelection().Clear();
				EditorContext::GetInstance()->MarkLevelDirty();
			};

		EditorCommandHistory::GetInstance()->Clear();
		EditorCommandHistory::GetInstance()->PushExecuted(std::make_unique<EditorLambdaCommand>(
			"コンポーネント追加",
			[applyState, afterState]() { applyState(afterState); },
			[applyState, beforeState]()
			{
				applyState(beforeState);
				EditorCommandHistory::GetInstance()->DiscardDependentRedoCommands(); // 再生成前のComponentを参照する後続Redoを除去する。
			}));
		EditorContext::GetInstance()->MarkLevelDirty();
	}

	void ActorWorld::DeleteSelectedComponent()
	{
		if (!selectedComponent_)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no selected Component";
			return;
		}

		Actor* owner = selectedComponent_->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : no owner";
			return;
		}
		if (selectedComponent_ == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : RootComponent cannot be deleted";
			return;
		}

		const std::string beforeState = ActorJsonSerializer::SerializeActor(*owner).dump();
		const std::string deletedComponentName = selectedComponent_->GetName();
		const bool wasPhysicsRegistered = owner->IsPhysicsRegistered();
		if (wasPhysicsRegistered) UnregisterPhysicsComponents(*owner);

		ActorComponent* deleteTarget = selectedComponent_;
		selectedComponent_ = nullptr;
		selectedActor_ = owner;
		EditorContext::GetInstance()->GetSelection().Clear();
		const bool removed = owner->RemoveComponent(deleteTarget);
		if (wasPhysicsRegistered) RegisterPhysicsComponents(*owner);
		if (!removed)
		{
			lastActorJsonSaveMessage_ = "Delete Component failed : " + deletedComponentName;
			return;
		}

		const std::string afterState = ActorJsonSerializer::SerializeActor(*owner).dump();
		auto applyState = [this, owner](const std::string& stateText)
			{
				const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
				if (state.is_discarded()) return;
				UnregisterPhysicsComponents(*owner);
				ActorJsonSerializer::LoadActorFromJson(*owner, state);
				RegisterPhysicsComponents(*owner);
				selectedActor_ = owner;
				selectedComponent_ = nullptr;
				EditorContext::GetInstance()->GetSelection().Clear();
				EditorContext::GetInstance()->MarkLevelDirty();
			};

		EditorCommandHistory::GetInstance()->Clear();
		EditorCommandHistory::GetInstance()->PushExecuted(std::make_unique<EditorLambdaCommand>(
			"コンポーネント削除",
			[applyState, afterState]() { applyState(afterState); },
			[applyState, beforeState]()
			{
				applyState(beforeState);
				EditorCommandHistory::GetInstance()->DiscardDependentRedoCommands();
			}));
		lastActorJsonSaveMessage_ = "Deleted Component : " + deletedComponentName;
		EditorContext::GetInstance()->MarkLevelDirty();
	}

	void ActorWorld::DuplicateSelectedComponent()
	{
		if (!selectedComponent_)
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : no selected Component";
			return;
		}

		Actor* owner = selectedComponent_->GetOwner();
		if (!owner)
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : no owner Actor";
			return;
		}
		if (selectedComponent_ == owner->GetRootComponent())
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : RootComponent cannot be duplicated";
			return;
		}

		nlohmann::json sourceJson;
		selectedComponent_->ToJson(sourceJson);
		if (!sourceJson.contains("Class") || !sourceJson["Class"].is_string())
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : invalid Component class";
			return;
		}
		const std::string className = sourceJson["Class"].get<std::string>();
		if (!ComponentFactory::IsAllowMultiple(className) && owner->HasComponentClass(className))
		{
			lastActorJsonSaveMessage_ = "Duplicate Component failed : already exists " + className;
			return;
		}

		const std::string beforeState = ActorJsonSerializer::SerializeActor(*owner).dump();
		const bool wasPhysicsRegistered = owner->IsPhysicsRegistered();
		if (wasPhysicsRegistered) UnregisterPhysicsComponents(*owner);

		ActorComponent* duplicatedComponent = ComponentFactory::CreateComponent(owner, className);
		if (!duplicatedComponent)
		{
			if (wasPhysicsRegistered) RegisterPhysicsComponents(*owner);
			lastActorJsonSaveMessage_ = "Duplicate Component failed : " + className;
			return;
		}

		duplicatedComponent->FromJson(sourceJson);
		const std::string baseName = selectedComponent_->GetName().empty() ? className : selectedComponent_->GetName();
		duplicatedComponent->SetName(MakeUniqueComponentName(*owner, baseName + "_Copy"));
		if (SceneComponent* duplicatedScene = dynamic_cast<SceneComponent*>(duplicatedComponent))
		{
			SceneComponent* sourceScene = dynamic_cast<SceneComponent*>(selectedComponent_);
			if (sourceScene && sourceScene->GetParent()) duplicatedScene->AttachTo(sourceScene->GetParent());
			else if (owner->GetRootComponent() && owner->GetRootComponent() != duplicatedScene) duplicatedScene->AttachTo(owner->GetRootComponent());
			duplicatedScene->RefreshWorldTransform();
		}
		if (isInitialized_) duplicatedComponent->InitializeForWorld();
		if (wasPhysicsRegistered) RegisterPhysicsComponents(*owner);

		selectedActor_ = nullptr;
		selectedComponent_ = duplicatedComponent;
		requestFocusActorDetails_ = true;
		lastActorJsonSaveMessage_ = "Duplicated Component : " + duplicatedComponent->GetName();

		const std::string afterState = ActorJsonSerializer::SerializeActor(*owner).dump();
		auto applyState = [this, owner](const std::string& stateText)
			{
				const nlohmann::json state = nlohmann::json::parse(stateText, nullptr, false);
				if (state.is_discarded()) return;
				UnregisterPhysicsComponents(*owner);
				ActorJsonSerializer::LoadActorFromJson(*owner, state);
				RegisterPhysicsComponents(*owner);
				selectedActor_ = owner;
				selectedComponent_ = nullptr;
				EditorContext::GetInstance()->GetSelection().Clear();
				EditorContext::GetInstance()->MarkLevelDirty();
			};

		EditorCommandHistory::GetInstance()->Clear();
		EditorCommandHistory::GetInstance()->PushExecuted(std::make_unique<EditorLambdaCommand>(
			"コンポーネント複製",
			[applyState, afterState]() { applyState(afterState); },
			[applyState, beforeState]()
			{
				applyState(beforeState);
				EditorCommandHistory::GetInstance()->DiscardDependentRedoCommands();
			}));
		EditorContext::GetInstance()->MarkLevelDirty();
	}
} // namespace Ken4lowEngine
