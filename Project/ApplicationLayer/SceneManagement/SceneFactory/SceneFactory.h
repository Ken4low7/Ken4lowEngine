#pragma once
#include "AbstractSceneFactory.h"

#include <functional>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///				　		ゲーム用のシーン工場
	/// -------------------------------------------------------------
	class SceneFactory : public AbstractSceneFactory
	{
	public:
		using SceneCreator = std::function<std::unique_ptr<BaseScene>()>;

		/// <summary>Scene ClassをFactoryへ登録します。</summary>
		static bool RegisterSceneClass(std::string sceneName, SceneCreator creator);

		/// <summary>登録済みScene ClassからSceneを生成します。</summary>
		std::unique_ptr<BaseScene> CreateScene(const std::string& sceneName) override;

		bool CanCreateScene(const std::string& sceneName) const override;
		std::vector<std::string> GetRegisteredSceneNames() const override;
	};

} // namespace Ken4lowEngine

#define K4E_SCENE_REGISTRY_CONCAT_IMPL(lhs, rhs) lhs##rhs
#define K4E_SCENE_REGISTRY_CONCAT(lhs, rhs) K4E_SCENE_REGISTRY_CONCAT_IMPL(lhs, rhs)
#define K4E_REGISTER_SCENE_NAMED(SceneName, SceneType) \
	namespace { [[maybe_unused]] const bool K4E_SCENE_REGISTRY_CONCAT(g_k4eSceneRegistered_, __COUNTER__) = \
		::Ken4lowEngine::SceneFactory::RegisterSceneClass(SceneName, [] { return std::make_unique<SceneType>(); }); }
#define K4E_REGISTER_SCENE(SceneType) K4E_REGISTER_SCENE_NAMED(#SceneType, SceneType) // Scene側に1行書くだけでFactoryとEditor一覧へ自動登録する。
