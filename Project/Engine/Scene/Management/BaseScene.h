#pragma once

#include "SceneDefinition.h"
#include <SceneLevelLoader.h>

#include <Editor/EditorObjectInfo.h>

#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class ActorWorld;
	class SceneManager;

	/// -------------------------------------------------------------
	///　　　　　　　　　　シーンの基底クラス
	/// -------------------------------------------------------------
	enum class EditorInputPolicy
	{
		UiMouse,
		FpsCapture,
	};

	class BaseScene
	{
	public:
		virtual ~BaseScene() = default;
		virtual void Initialize() = 0;
		virtual void Update() = 0;

		/// <summary>SceneManagerがJSON定義をInitialize前に適用します。</summary>
		virtual void ApplySceneDefinition(const SceneDefinition& definition)
		{
			sceneDefinition_ = definition; // C++ Scene固有処理を残しつつ、使用Levelや遷移先をデータから参照できるようにする。
		}

		[[nodiscard]] const SceneDefinition& GetSceneDefinition() const { return sceneDefinition_; }

		/// <summary>Editor Mode中にゲーム進行を止めたまま確認用更新だけ行う。</summary>
		virtual void UpdateEditor(float /*deltaTime*/) {}

		/// <summary>EditorのPlay開始直前に編集状態を保存する。</summary>
		virtual void BeginEditorPlay() {}

		/// <summary>EditorのStop時にPlay前の編集状態へ戻す。</summary>
		virtual void EndEditorPlay() {} // Play用の一時変化をEdit Worldへ残さないための復元入口。

		virtual void PrepareShadowPass() {}
		virtual void Draw3DObjects() = 0;
		virtual void DrawShadowObjects() = 0;
		virtual void Draw2DSprites() = 0;
		virtual void Finalize() = 0;
		virtual void DrawImGui() = 0;
		virtual EditorInputPolicy GetEditorInputPolicy() const { return EditorInputPolicy::UiMouse; }
		virtual void CollectEditorObjects(std::vector<Ken4lowEngine::EditorObjectInfo>& /*outObjects*/) {}
		virtual ActorWorld* GetEditorActorWorld() { return nullptr; }

		/// <summary>SceneDefinitionのLevelを読み込むActorWorldを返します。</summary>
		virtual ActorWorld* GetSceneActorWorld()
		{
			return GetEditorActorWorld(); // 既存のActorWorld対応Sceneは追加実装なしでLevel自動読込へ参加できる。
		}

		virtual void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

		/// <summary>Scene固有ロード開始時にSceneDefinitionのLevelを自動適用します。</summary>
		virtual void StartLoad()
		{
			lastLevelLoadAttempted_ = false;
			lastLevelLoadSucceeded_ = true;
			lastLevelLoadMessage_.clear();
			if (sceneDefinition_.levelPath.empty()) return;

			lastLevelLoadAttempted_ = true;
			ActorWorld* actorWorld = GetSceneActorWorld();
			if (!actorWorld)
			{
				lastLevelLoadSucceeded_ = false;
				lastLevelLoadMessage_ = "SceneはLevel読込用ActorWorldを公開していません: " + sceneDefinition_.id;
				return;
			}

			const SceneLevelLoader::Result result = SceneLevelLoader::Load(sceneDefinition_.levelPath, *actorWorld);
			lastLevelLoadSucceeded_ = result.succeeded;
			lastLevelLoadMessage_ = result.message; // SceneManagerやDebug UIが同じ結果を参照できるようSceneへ保持する。
		}

		[[nodiscard]] bool WasLevelLoadAttempted() const { return lastLevelLoadAttempted_; }
		[[nodiscard]] bool DidLevelLoadSucceed() const { return lastLevelLoadSucceeded_; }
		[[nodiscard]] const std::string& GetLastLevelLoadMessage() const { return lastLevelLoadMessage_; }

		virtual void UpdateLoad() {};
		virtual void StartUnload() {};
		virtual void UpdateUnload() {};
		virtual bool IsReadyToStartUncover() const { return true; }
		virtual bool IsReadyToSwapOut() const { return true; }

	protected:
		SceneManager* sceneManager_ = nullptr;
		SceneDefinition sceneDefinition_{};
		bool lastLevelLoadAttempted_ = false;
		bool lastLevelLoadSucceeded_ = true;
		std::string lastLevelLoadMessage_;
	};
} // namespace Ken4lowEngine
