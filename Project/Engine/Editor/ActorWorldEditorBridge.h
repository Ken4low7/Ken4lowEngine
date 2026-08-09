#pragma once

#include "EditorContext.h"
#include "EditorObjectInfo.h"

#include <ActorWorld.h>
#include <InstancedModelComponent.h>

#include <cstdint>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	bool BuildInstancedModelInstanceEditorInfo(
		InstancedModelComponent* component,
		size_t instanceIndex,
		uint64_t componentId,
		std::string_view sceneName,
		EditorObjectInfo& outInfo);
	
	/// <summary>Actor / ComponentをWorld OutlinerとDetails用の軽量情報へ変換します。</summary>
	void CollectActorWorldEditorObjects(
		ActorWorld& actorWorld,
		std::vector<EditorObjectInfo>& outObjects,
		std::string_view sceneName);

} // namespace Ken4lowEngine
