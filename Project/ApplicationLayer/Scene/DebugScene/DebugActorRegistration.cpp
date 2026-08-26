#include "DebugActorRegistration.h"

#include <ActorFactory.h>
#include <ComponentFactory.h>
#include <SceneComponent.h>

#include "BasicParticleActor.h"
#include "TestGroundActor.h"

using namespace Ken4lowEngine;

namespace
{
	template<class T>
	ComponentFactory::ComponentTypeInfo MakeApplicationComponentTypeInfo(const char* className, const char* displayName, const char* category, const char* description)
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = className;
		typeInfo.displayName = displayName;
		typeInfo.category = category;
		typeInfo.description = description;
		typeInfo.allowMultiple = false;
		typeInfo.canBeRoot = false;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent* { return owner ? &owner->AddComponent<T>() : nullptr; };
		return typeInfo;
	}

	template<class T>
	ComponentFactory::ComponentTypeInfo MakeApplicationSceneComponentTypeInfo(const char* className, const char* displayName, const char* category, const char* description)
	{
		ComponentFactory::ComponentTypeInfo typeInfo{};
		typeInfo.className = className;
		typeInfo.displayName = displayName;
		typeInfo.category = category;
		typeInfo.description = description;
		typeInfo.allowMultiple = false;
		typeInfo.canBeRoot = true;
		typeInfo.createFunc = [](Actor* owner) -> ActorComponent* { return owner ? &owner->AddComponent<T>() : nullptr; };
		typeInfo.createRootFunc = [](Actor* owner) -> SceneComponent* { return owner ? &owner->CreateRootComponent<T>() : nullptr; };
		return typeInfo;
	}
}

void RegisterApplicationActorTypes()
{
	static bool registered = false;
	if (registered) return;
	registered = true; // DebugSceneとGamePlaySceneの両方から呼ばれてもFactory登録を一度だけ行う。

	// JSONへ保存されるClass名と同じ文字列でApplication ActorをFactory登録する。
	ActorFactory::RegisterActorClass<BasicParticleActor>("BasicParticle");
	ActorFactory::RegisterActorClass<TestGroundActor>("TestGroundActor");
}

void RegisterDebugActors()
{
	RegisterApplicationActorTypes();
}
