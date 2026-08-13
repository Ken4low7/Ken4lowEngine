#pragma once

#include <ActorWorld.h>
#include <Editor/ActorWorldEditorBridge.h>
#include <GameTimer.h>
#include <BaseScene.h>

#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Editorでレベル制作を始めるための最小ActorWorld Sceneです。</summary>
	class SampleScene final : public BaseScene
	{
	public:
		void Initialize() override
		{
			actorWorld_.Initialize(); // 空SceneでもPlace Actorsからすぐ編集を始められるActorWorldを用意する。
		}

		void Update() override
		{
			actorWorld_.Update(GameTimer::GetInstance()->GetDeltaTime());
		}

		void UpdateEditor(float deltaTime) override
		{
			actorWorld_.UpdateEditor(deltaTime);
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
			CollectActorWorldEditorObjects(actorWorld_, outObjects, "SampleScene");
		}

		ActorWorld* GetEditorActorWorld() override
		{
			return &actorWorld_;
		}

	private:
		ActorWorld actorWorld_;
	};
} // namespace Ken4lowEngine
