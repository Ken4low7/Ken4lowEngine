#include "SceneFactory.h"
#include "DebugScene.h"
#include "../../Scene/SampleScene/SampleScene.h"
#include "../../../Engine/Scene/Management/DataDrivenScene.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace Ken4lowEngine
{
	namespace
	{
		using SceneCreatorMap = std::unordered_map<std::string, SceneFactory::SceneCreator>;

		SceneCreatorMap& GetSceneCreators()
		{
			static SceneCreatorMap creators;
			return creators; // Function-local staticなら各Sceneの静的登録順に依存せず安全に初期化できる。
		}
	}

	bool SceneFactory::RegisterSceneClass(std::string sceneName, SceneCreator creator)
	{
		if (sceneName.empty() || !creator) return false;
		auto& creators = GetSceneCreators();
		return creators.emplace(std::move(sceneName), std::move(creator)).second;
	}

	std::unique_ptr<BaseScene> SceneFactory::CreateScene(const std::string& sceneName)
	{
		const auto& creators = GetSceneCreators();
		const auto iterator = creators.find(sceneName);
		if (iterator == creators.end())
		{
			throw std::runtime_error("Unknown scene class: " + sceneName);
		}
		return iterator->second();
	}

	bool SceneFactory::CanCreateScene(const std::string& sceneName) const
	{
		return GetSceneCreators().contains(sceneName);
	}

	std::vector<std::string> SceneFactory::GetRegisteredSceneNames() const
	{
		std::vector<std::string> names;
		names.reserve(GetSceneCreators().size());
		for (const auto& [name, creator] : GetSceneCreators())
		{
			(void)creator;
			names.push_back(name);
		}
		std::sort(names.begin(), names.end());
		return names;
	}
} // namespace Ken4lowEngine

K4E_REGISTER_SCENE_NAMED("DataDrivenScene", ::Ken4lowEngine::DataDrivenScene) // 通常Sceneはこの1 Classを共有し、Level JSONだけで増やせる。
K4E_REGISTER_SCENE_NAMED("SampleScene", ::Ken4lowEngine::SampleScene)
#ifdef _DEBUG
K4E_REGISTER_SCENE_NAMED("DebugScene", ::DebugScene)
#endif
