#include "ActorJsonSerializer.h"
#include "Actor.h"
#include "ActorComponent.h"
#include "SceneComponent.h"
#include "ComponentFactory.h"
#include "ActorFactory.h"
#include <Scene/Actor/Character/HumanoidCharacterActor.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		const ComponentFactory::ComponentTypeInfo* FindRegisteredComponentType(std::string_view className)
		{
			for (const ComponentFactory::ComponentTypeInfo& typeInfo : ComponentFactory::GetRegisteredComponentTypes())
			{
				if (typeInfo.className == className) return &typeInfo;
			}
			return nullptr;
		}

		bool ValidateActorJson(const nlohmann::json& actorJson)
		{
			if (!actorJson.is_object() || !actorJson.contains("Components") || !actorJson["Components"].is_array()) return false;

			std::unordered_map<std::string, std::size_t> componentClassCounts;
			std::unordered_map<std::string, std::string> parentBySceneComponentName;
			std::size_t rootComponentCount = 0;

			for (const auto& componentJson : actorJson["Components"])
			{
				if (!componentJson.is_object() || !componentJson.contains("Class") || !componentJson["Class"].is_string()) return false;

				const std::string className = componentJson["Class"].get<std::string>();
				const ComponentFactory::ComponentTypeInfo* typeInfo = FindRegisteredComponentType(className);
				if (!typeInfo) return false;

				const std::size_t classCount = ++componentClassCounts[className];
				if (!typeInfo->allowMultiple && classCount > 1) return false; // UI制限だけでなくJSON読込時も単一Component制約を保証する。

				if (componentJson.contains("Type") && componentJson["Type"].is_string())
				{
					const bool declaredSceneComponent = componentJson["Type"].get<std::string>() == "SceneComponent";
					if (declaredSceneComponent != typeInfo->canBeRoot) return false;
				}

				if (!typeInfo->canBeRoot) continue;

				const std::string componentName = componentJson.value("Name", std::string{});
				const std::string parentName = componentJson.value("Parent", std::string{});
				if (componentName.empty() || !parentBySceneComponentName.emplace(componentName, parentName).second) return false;
				if (parentName.empty()) ++rootComponentCount;
			}

			if (!parentBySceneComponentName.empty() && rootComponentCount != 1) return false;
			for (const auto& [componentName, parentName] : parentBySceneComponentName)
			{
				if (parentName.empty()) continue;
				if (parentName == componentName || !parentBySceneComponentName.contains(parentName)) return false;
			}

			for (const auto& [componentName, unusedParentName] : parentBySceneComponentName)
			{
				(void)unusedParentName;
				std::unordered_set<std::string> ancestry;
				std::string currentName = componentName;
				while (!currentName.empty())
				{
					if (!ancestry.insert(currentName).second) return false;
					const auto parentIt = parentBySceneComponentName.find(currentName);
					currentName = parentIt != parentBySceneComponentName.end() ? parentIt->second : std::string{};
				}
			}
			return true;
		}

		std::unordered_set<ActorComponent*> PrepareComponentsForReload(Actor& actor)
		{
			std::unordered_set<ActorComponent*> reusableComponents;
			auto* humanoid = dynamic_cast<HumanoidCharacterActor*>(&actor);
			if (!humanoid)
			{
				actor.ClearComponents();
				return reusableComponents;
			}

			if (SceneComponent* root = actor.GetRootComponent()) reusableComponents.insert(root);
			if (HumanoidVisualComponent* visual = humanoid->GetHumanoidVisualComponent()) reusableComponents.insert(visual);

			std::vector<ActorComponent*> removeTargets;
			for (const auto& ownedComponent : actor.GetComponents())
			{
				ActorComponent* component = ownedComponent.get();
				if (component && !reusableComponents.contains(component)) removeTargets.push_back(component);
			}
			for (ActorComponent* component : removeTargets) actor.RemoveComponent(component);
			for (ActorComponent* component : reusableComponents)
			{
				if (component) component->FinalizeForWorld(); // 部位参照が指す実体は残し、GPU・階層状態だけを再初期化可能にする。
			}
			return reusableComponents;
		}
	}

	nlohmann::json ActorJsonSerializer::SerializeActor(const Actor& actor)
	{
		nlohmann::json actorJson;
		actor.ToJson(actorJson); // Command履歴はファイルを経由せずActor構成を保持する。
		return actorJson;
	}

	bool ActorJsonSerializer::ValidateActorDefinition(const nlohmann::json& actorJson)
	{
		if (!ValidateActorJson(actorJson) || !actorJson.contains("Class") || !actorJson["Class"].is_string()) return false;
		return ActorFactory::IsRegistered(actorJson["Class"].get<std::string>()); // World破棄前にActorと全ComponentのFactory登録を検証する。
	}

	bool ActorJsonSerializer::SaveActorToFile(const Actor& actor, std::string_view filePath)
	{
		try
		{
			const std::filesystem::path path(filePath);
			if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());

			std::ofstream file(path);
			if (!file.is_open()) return false;
			file << SerializeActor(actor).dump(4);
			return file.good();
		}
		catch (...)
		{
			return false; // ファイルシステム例外をScene更新へ伝播させない。
		}
	}

	bool ActorJsonSerializer::LoadActorFromFile(Actor& actor, std::string_view filePath)
	{
		try
		{
			std::ifstream file{ std::filesystem::path{ std::string(filePath) } };
			if (!file.is_open()) return false;

			nlohmann::json actorJson;
			file >> actorJson;
			return LoadActorFromJson(actor, actorJson);
		}
		catch (...)
		{
			return false; // JSON構文エラー時は既存Actorを変更せず失敗を返す。
		}
	}

	bool ActorJsonSerializer::LoadActorFromJson(Actor& actor, const nlohmann::json& actorJson)
	{
		if (!ValidateActorJson(actorJson)) return false;

		std::unordered_set<ActorComponent*> reusableComponents = PrepareComponentsForReload(actor);
		actor.FromJson(actorJson);

		std::unordered_map<std::string, SceneComponent*> sceneComponentsByName;
		std::vector<std::pair<SceneComponent*, std::string>> pendingAttachments;
		std::unordered_set<ActorComponent*> reusedComponents;

		for (const auto& componentJson : actorJson["Components"])
		{
			const std::string className = componentJson["Class"].get<std::string>();
			const ComponentFactory::ComponentTypeInfo* typeInfo = FindRegisteredComponentType(className);
			const std::string parentName = componentJson.value("Parent", std::string{});

			ActorComponent* createdComponent = nullptr;
			for (ActorComponent* reusable : reusableComponents)
			{
				if (!reusable || reusedComponents.contains(reusable)) continue;
				if (reusable->GetClassTypeName() != className) continue;
				createdComponent = reusable;
				reusedComponents.insert(reusable);
				break;
			}

			if (!createdComponent)
			{
				if (typeInfo && typeInfo->canBeRoot && parentName.empty())
				{
					createdComponent = ComponentFactory::CreateRootSceneComponent(&actor, className);
				}
				else
				{
					createdComponent = ComponentFactory::CreateComponent(&actor, className);
				}
			}
			if (!createdComponent) return false;

			createdComponent->FromJson(componentJson);
			if (SceneComponent* sceneComponent = dynamic_cast<SceneComponent*>(createdComponent))
			{
				sceneComponentsByName.emplace(sceneComponent->GetName(), sceneComponent);
				if (!parentName.empty()) pendingAttachments.emplace_back(sceneComponent, parentName);
				else actor.SetRootComponent(sceneComponent);
			}
		}

		for (const auto& [child, parentName] : pendingAttachments)
		{
			const auto parentIt = sceneComponentsByName.find(parentName);
			if (!child || parentIt == sceneComponentsByName.end()) return false;
			child->AttachTo(parentIt->second);
		}

		actor.InitializeComponents();
		return true;
	}

	std::unique_ptr<Actor> ActorJsonSerializer::CreateActorFromJson(std::string_view filePath, const ActorSpawnOptions& options)
	{
		try
		{
			std::ifstream file{ std::filesystem::path{ std::string(filePath) } };
			if (!file.is_open()) return nullptr;

			nlohmann::json actorJson;
			file >> actorJson;
			return CreateActorFromJson(actorJson, options);
		}
		catch (...)
		{
			return nullptr; // 不正JSONから半端なActorをWorldへ追加しない。
		}
	}

	std::unique_ptr<Actor> ActorJsonSerializer::CreateActorFromJson(const nlohmann::json& actorJson, const ActorSpawnOptions& options)
	{
		std::unique_ptr<Actor> actor;
		try
		{
			if (!ValidateActorDefinition(actorJson)) return nullptr;

			actor = ActorFactory::CreateActor(actorJson["Class"].get<std::string>());
			if (!actor) return nullptr;
			if (!LoadActorFromJson(*actor, actorJson))
			{
				actor->FinalizeForWorld();
				return nullptr;
			}

			if (options.applySpawnOffset && actor->GetRootComponent())
			{
				SceneComponent* root = actor->GetRootComponent();
				root->SetLocalPosition(Vector3::Add(root->GetLocalPosition(), options.spawnOffset));
				root->RefreshWorldTransform(); // Actor生成CommandのRedoでも同じSpawnOffsetを再現する。
			}
			return actor;
		}
		catch (...)
		{
			if (actor) actor->FinalizeForWorld();
			return nullptr; // Component生成・初期化失敗時に不完全なActorを返さない。
		}
	}
} // namespace Ken4lowEngine
