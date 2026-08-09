#include "SceneFactory.h"
#include "TitleScene.h"
#include "GamePlayScene.h"
#include "StageSelectScene.h"
#include "DebugScene.h"

#include <functional>
#include <unordered_map>

namespace Ken4lowEngine
{
	namespace
	{
		using SceneCreator = std::function<std::unique_ptr<BaseScene>()>;

		const std::unordered_map<std::string, SceneCreator>& GetSceneCreators()
		{
			static const std::unordered_map<std::string, SceneCreator> creators = []
				{
					std::unordered_map<std::string, SceneCreator> result;
					result.emplace("TitleScene", [] { return std::make_unique<TitleScene>(); });
					result.emplace("StageSelectScene", [] { return std::make_unique<StageSelectScene>(); });
					result.emplace("GamePlayScene", [] { return std::make_unique<GamePlayScene>(); });
#ifdef _DEBUG
					result.emplace("DebugScene", [] { return std::make_unique<DebugScene>(); });
#endif
					return result; // JSONのScene IDとC++ Class名を分離できるようClass生成だけを登録表へ集約する。
				}();
			return creators;
		}
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
} // namespace Ken4lowEngine
