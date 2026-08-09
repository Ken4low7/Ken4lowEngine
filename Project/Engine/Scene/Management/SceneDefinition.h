#pragma once

#include <json.hpp>

#include <exception>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>Scene遷移や使用Levelなど、C++ Sceneへ適用するデータ定義です。</summary>
	struct SceneDefinition
	{
		struct TransitionSettings
		{
			std::string type = "Fade";
			float duration = 1.0f;
		};

		std::string id;
		std::string className;
		std::string levelPath;
		std::string gameMode;
		std::string playerActor;
		std::string uiLayout;
		std::string bgmPath;
		std::string nextScene;
		std::string retryScene;
		bool editorOnly = false;
		TransitionSettings transition{};
		nlohmann::json parameters = nlohmann::json::object();

		[[nodiscard]] bool IsValid() const
		{
			return !id.empty() && !className.empty(); // Scene IDと生成するC++ Classは必須項目として扱う。
		}
	};
} // namespace Ken4lowEngine
