#pragma once

#include "BaseScene.h"

#include <ActorWorld.h>
#include <Editor/ActorWorldEditorBridge.h>
#include <GameTimer.h>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// SceneDefinition + Level JSONだけで通常のActorWorld Sceneを構築する汎用Sceneです。
	/// Title / Gameplay / Resultなど、専用C++処理が不要なSceneはこのClassを共有します。
	/// </summary>
	class DataDrivenScene final : public BaseScene
	{
	public:
		void Initialize() override
		{
			actorWorld_.Initialize(); // SceneごとのC++ Classを作らず、共通ActorWorldを編集・実行できる状態にする。
		}

		void Update() override
		{
			actorWorld_.Update(GameTimer::GetInstance()->GetDeltaTime());
		}

		void UpdateEditor(float /*deltaTime*/) override
		{
			// Editor ActorWorldの更新はSceneManager::RefreshEditorVisualStateへ集約する。
		}

		void PrepareShadowPass() override
		{
			actorWorld_.PrepareRenderState();
		}

		void Draw3DObjects() override
		{
			actorWorld_.Draw();
		}

		void DrawShadowObjects() override
		{
			actorWorld_.DrawShadow();
		}

		void Draw2DSprites() override
		{
			actorWorld_.DrawScreenSpaceSprites();
		}

		void Finalize() override
		{
			actorWorld_.Finalize();
		}

		void DrawImGui() override
		{
			actorWorld_.DrawImGui();
		}

		void CollectEditorObjects(std::vector<EditorObjectInfo>& outObjects) override
		{
			const std::string sceneName = sceneDefinition_.id.empty() ? "DataDrivenScene" : sceneDefinition_.id;
			CollectActorWorldEditorObjects(actorWorld_, outObjects, sceneName);
		}

		ActorWorld* GetEditorActorWorld() override
		{
			return &actorWorld_;
		}

	private:
		ActorWorld actorWorld_;
	};
} // namespace Ken4lowEngine
