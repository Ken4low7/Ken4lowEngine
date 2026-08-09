#define NOMINMAX
#include "SceneManager.h"

#include <ActorComponent.h>
#include <ActorWorld.h>
#include <SpriteManager.h>
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <Editor/EditorCommandHistory.h>
#include <Editor/EditorContext.h>
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#include <Editor/EditorPlaySessionManager.h>
#include <Editor/EditorWindowManager.h>
#endif

#include <algorithm>
#include <cassert>
#include <exception>
#include <string>
#include <vector>

namespace K4E = ::Ken4lowEngine;

namespace Ken4lowEngine
{
	SceneManager::SceneManager() = default;
	SceneManager::~SceneManager() = default;

	void SceneManager::Initialize()
	{
		if (sceneTransition_) sceneTransition_->Initialize();
		isTransitioning_ = false;
		sceneSwapped_ = false;
		pendingCrack_ = false;
		coverHoldCounter_ = 0;
		uncoverDelayCounter_ = 0;
		unloadRequested_ = false;
		editorPlaySessionActive_ = false;
		editorPlayUsesSceneRecreate_ = false;
		editorPlayOriginSceneId_.clear();
		editorSingleStepRequested_ = false;
	}

	bool SceneManager::LoadSceneDefinitions(const std::string& registryPath)
	{
		const bool loaded = sceneDefinitionRegistry_.Load(registryPath);
#ifdef USE_IMGUI
		K4E::EditorWindowManager::GetInstance()->AddOutputLog(
			loaded ? K4E::EditorLogLevel::Info : K4E::EditorLogLevel::Warning,
			loaded
				? "Scene Registryを読み込みました: " + registryPath
				: "Scene Registryの一部または全部を読み込めなかったためFallback定義を使用します: " + sceneDefinitionRegistry_.GetLastError());
#endif
		return loaded; // 読込失敗時もRegistry内部のFallback定義で従来Scene名を維持する。
	}

	std::string SceneManager::GetStartupSceneName(bool debugBuild) const
	{
		return sceneDefinitionRegistry_.GetStartupScene(debugBuild);
	}

	const SceneDefinition* SceneManager::FindSceneDefinition(const std::string& sceneId) const
	{
		return sceneDefinitionRegistry_.Find(sceneId);
	}

	SceneDefinition SceneManager::ResolveSceneDefinition(const std::string& sceneId) const
	{
		if (const SceneDefinition* definition = sceneDefinitionRegistry_.Find(sceneId))
		{
			return *definition;
		}

		SceneDefinition fallback{};
		fallback.id = sceneId;
		fallback.className = sceneId; // 未登録名は従来互換としてC++ Scene Class名と同一扱いにする。
		return fallback;
	}

	void SceneManager::ProcessEditorPlayRequests()
	{
#ifdef USE_IMGUI
		if (!scene_) return;

		K4E::EditorPlayController* playController = K4E::EditorPlayController::GetInstance();
		K4E::EditorPlaySessionManager* sessionManager = K4E::EditorPlaySessionManager::GetInstance();
		K4E::EditorWindowManager* windowManager = K4E::EditorWindowManager::GetInstance();
		const K4E::EditorPlayRequest request = playController->ConsumePendingRequest();

		switch (request)
		{
		case K4E::EditorPlayRequest::Start:
			if (editorPlaySessionActive_)
			{
				playController->CommitPlayResumed();
				break;
			}

			if (scene_->GetEditorActorWorld())
			{
				if (sessionManager->BeginPlaySession(*scene_))
				{
					scene_->BeginEditorPlay();
					editorPlaySessionActive_ = true;
					editorPlayUsesSceneRecreate_ = false;
					editorPlayOriginSceneId_.clear();
					editorSingleStepRequested_ = false;
					playController->CommitPlayStarted();
				}
				else
				{
					playController->CommitStopped();
				}
			}
			else
			{
				editorPlayOriginSceneId_ = !currentSceneDefinition_.id.empty()
					? currentSceneDefinition_.id
					: currentSceneDefinition_.className;
				scene_->BeginEditorPlay();
				editorPlaySessionActive_ = true;
				editorPlayUsesSceneRecreate_ = true;
				editorSingleStepRequested_ = false;
				playController->CommitPlayStarted();
				windowManager->AddOutputLog(
					K4E::EditorLogLevel::Info,
					"このSceneはEditor ActorWorldを公開していないため、Stop時にSceneを再生成するPIE互換モードで開始しました。"); // 旧Sceneも再生可能にしつつRuntime変更はStop時の再生成で破棄する。
			}
			break;

		case K4E::EditorPlayRequest::Resume:
			if (editorPlaySessionActive_)
			{
				playController->CommitPlayResumed();
			}
			else
			{
				playController->CommitStopped();
			}
			break;

		case K4E::EditorPlayRequest::Pause:
			if (editorPlaySessionActive_) playController->CommitPaused();
			break;

		case K4E::EditorPlayRequest::Step:
			if (editorPlaySessionActive_)
			{
				playController->CommitPaused();
				editorSingleStepRequested_ = true; // Pause状態を保ったままこのUpdateだけRuntime Tickを許可する。
			}
			break;

		case K4E::EditorPlayRequest::Stop:
		case K4E::EditorPlayRequest::KeepChangesAndStop:
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				const bool keepChanges = request == K4E::EditorPlayRequest::KeepChangesAndStop;

				if (editorPlayUsesSceneRecreate_)
				{
					const std::string reloadSceneId = !editorPlayOriginSceneId_.empty()
						? editorPlayOriginSceneId_
						: (!currentSceneDefinition_.id.empty() ? currentSceneDefinition_.id : currentSceneDefinition_.className);
					editorPlaySessionActive_ = false;
					editorPlayUsesSceneRecreate_ = false;
					editorPlayOriginSceneId_.clear();
					editorSingleStepRequested_ = false;
					playController->CommitStopped();

					if (keepChanges)
					{
						windowManager->AddOutputLog(
							K4E::EditorLogLevel::Warning,
							"Editor ActorWorldを公開しないSceneではRuntime変更の保持に対応していないため、通常Stopとして再生成します。");
					}
					windowManager->AddOutputLog(
						K4E::EditorLogLevel::Info,
						"PIE互換モードを停止し、Play開始元のScene定義からSceneを再生成します: " + reloadSceneId);
					ChangeScene(reloadSceneId); // Runtime中に別Sceneへ遷移していてもPlay開始元のEditor Sceneへ戻す。
				}
				else if (sessionManager->EndPlaySession(*scene_, keepChanges))
				{
					editorPlaySessionActive_ = false;
					editorSingleStepRequested_ = false;
					playController->CommitStopped();
				}
				else
				{
					playController->CommitPaused(); // 復元失敗時はRuntimeを進めず再操作できる状態で止める。
				}
			}
			else
			{
				playController->CommitStopped();
			}
			break;

		case K4E::EditorPlayRequest::None:
		default:
			break;
		}

		std::string statusMessage;
		bool statusSucceeded = false;
		if (sessionManager->ConsumeStatus(statusMessage, statusSucceeded))
		{
			windowManager->AddOutputLog(
				statusSucceeded ? K4E::EditorLogLevel::Info : K4E::EditorLogLevel::Error,
				statusMessage);
		}
#endif
	}

	void SceneManager::RefreshEditorVisualState(float deltaTime)
	{
#ifdef USE_IMGUI
		ActorWorld* actorWorld = scene_ ? scene_->GetEditorActorWorld() : nullptr;
		if (actorWorld) actorWorld->UpdateEditor(deltaTime); // Editor用Component更新もActorWorldの同じライフサイクル経路へ統一する。
#else
		(void)deltaTime;
#endif
	}

	void SceneManager::Update()
	{
		float dtRaw = K4E::GameTimer::GetInstance()->GetDeltaTime();
		float dtFade = (std::min)(dtRaw, 1.0f / 30.0f);
		if (sceneTransition_) sceneTransition_->Update(dtFade);

		if (isTransitioning_)
		{
			if (!sceneSwapped_)
			{
				if (sceneTransition_ && sceneTransition_->IsFullyCovered() && nextScene_)
				{
					if (scene_)
					{
						if (!unloadRequested_)
						{
							scene_->StartUnload();
							unloadRequested_ = true;
						}
						scene_->UpdateUnload();
					}
					const bool readyToSwap = (!scene_) || scene_->IsReadyToSwapOut();
					if (readyToSwap)
					{
						++coverHoldCounter_;
						if (coverHoldCounter_ >= coverHoldFrames_)
						{
							ApplyNextScene();
							sceneSwapped_ = true;
							pendingCrack_ = true;
							coverHoldCounter_ = 0;
							uncoverDelayCounter_ = 0;
							unloadRequested_ = false;
						}
					}
					else coverHoldCounter_ = 0;
				}
				else coverHoldCounter_ = 0;
			}

			if (pendingCrack_ && sceneTransition_ && sceneTransition_->IsFullyCovered() && scene_)
			{
				scene_->UpdateLoad();
				if (scene_->IsReadyToStartUncover())
				{
					++uncoverDelayCounter_;
					if (uncoverDelayCounter_ >= uncoverDelayFrames_)
					{
						sceneTransition_->StartCrack();
						pendingCrack_ = false;
						uncoverDelayCounter_ = 0;
					}
				}
				else uncoverDelayCounter_ = 0;
			}

			if (sceneTransition_ && !sceneTransition_->IsBusy())
			{
				isTransitioning_ = false;
				sceneSwapped_ = false;
				coverHoldCounter_ = 0;
				uncoverDelayCounter_ = 0;
				unloadRequested_ = false;
				if (hasQueuedChange_)
				{
					std::string name = queuedSceneName_;
					hasQueuedChange_ = false;
					queuedSceneName_.clear();
					ChangeScene(name);
				}
			}
		}

		if (scene_ && (!isTransitioning_ || sceneSwapped_))
		{
			bool shouldUpdateGame = true;
#ifdef USE_IMGUI
			K4E::EditorPlayController* playController = K4E::EditorPlayController::GetInstance();
			shouldUpdateGame = K4E::EditorModeController::GetInstance()->IsGamePreviewMode() ||
				playController->IsPlaying() || editorSingleStepRequested_;
#endif
			if (shouldUpdateGame)
			{
				scene_->Update();
#ifdef USE_IMGUI
				if (editorPlaySessionActive_)
				{
					K4E::EditorPlaySessionManager::GetInstance()->NotifyRuntimeTick(dtRaw, *scene_);
				}
#endif
			}
			else
			{
				scene_->UpdateEditor(dtRaw);
				RefreshEditorVisualState(dtRaw);
			}
		}
		editorSingleStepRequested_ = false;
	}

	void SceneManager::PrepareShadowPass() { if (scene_) scene_->PrepareShadowPass(); }
	void SceneManager::Draw3DObjects() { if (scene_) scene_->Draw3DObjects(); }
	void SceneManager::DrawShadowObjects() { if (scene_) scene_->DrawShadowObjects(); }

	void SceneManager::Draw2DSprites()
	{
		if (scene_) scene_->Draw2DSprites();
		if (sceneTransition_)
		{
			K4E::SpriteManager::GetInstance()->SetRenderSetting_UI();
			sceneTransition_->Draw2DSprites();
		}
	}

	void SceneManager::DrawImGui()
	{
		if (scene_) scene_->DrawImGui();
		if (sceneTransition_) sceneTransition_->DrawImGui();
	}

	void SceneManager::Finalize()
	{
		if (scene_)
		{
#ifdef USE_IMGUI
			if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				if (!editorPlayUsesSceneRecreate_)
				{
					K4E::EditorPlaySessionManager::GetInstance()->CancelSessionWithoutRestore();
				}
				editorPlaySessionActive_ = false;
				editorPlayUsesSceneRecreate_ = false;
				editorPlayOriginSceneId_.clear();
				K4E::EditorPlayController::GetInstance()->CommitStopped();
			}
#endif
			scene_->Finalize();
		}
		if (sceneTransition_) sceneTransition_->Finalize();
#ifdef USE_IMGUI
		K4E::EditorCommandHistory::GetInstance()->Clear();
#endif
	}

	void SceneManager::ChangeScene(const std::string& sceneId)
	{
		assert(sceneFactory_);
#ifdef USE_IMGUI
		const bool protectedActorWorldPie = editorPlaySessionActive_ && !editorPlayUsesSceneRecreate_;
		if (protectedActorWorldPie || K4E::EditorPlaySessionManager::GetInstance()->IsSessionActive())
		{
			K4E::EditorWindowManager::GetInstance()->AddOutputLog(
				K4E::EditorLogLevel::Warning,
				"ActorWorld Snapshot型PIE中のScene切り替えはEditor Worldを保護するため無効です。Stop後に切り替えてください。");
			return; // Scene再生成型PIEではRuntimeの通常Scene遷移を許可し、Snapshot型だけを保護する。
		}
#endif
		if (IsTransitioning())
		{
			queuedSceneName_ = sceneId;
			hasQueuedChange_ = true;
			return;
		}

		const SceneDefinition definition = ResolveSceneDefinition(sceneId);
#ifndef _DEBUG
		if (definition.editorOnly)
		{
#ifdef USE_IMGUI
			K4E::EditorWindowManager::GetInstance()->AddOutputLog(K4E::EditorLogLevel::Error, "ReleaseではEditorOnly Sceneへ遷移できません: " + sceneId);
#endif
			return;
		}
#endif

		try
		{
			nextScene_ = sceneFactory_->CreateScene(definition.className);
		}
		catch (const std::exception& exception)
		{
#ifdef USE_IMGUI
			K4E::EditorWindowManager::GetInstance()->AddOutputLog(
				K4E::EditorLogLevel::Error,
				"Scene生成に失敗しました: " + sceneId + " / " + exception.what());
#endif
			return;
		}
		if (!nextScene_) return;

		nextSceneDefinition_ = definition;
		nextScene_->ApplySceneDefinition(nextSceneDefinition_); // Initializeより前にLevel、BGM、遷移先などの定義を渡す。
		if (!sceneTransition_)
		{
			ApplyNextScene();
			return;
		}
		sceneTransition_->StartCover();
		isTransitioning_ = true;
		sceneSwapped_ = false;
		pendingCrack_ = false;
		coverHoldCounter_ = 0;
		uncoverDelayCounter_ = 0;
		unloadRequested_ = false;
	}

	void SceneManager::ApplyNextScene()
	{
		if (!nextScene_) return;
#ifdef USE_IMGUI
		K4E::EditorCommandHistory::GetInstance()->Clear();
		K4E::EditorContext::GetInstance()->ResetTransientState();
		const bool continueSceneRecreatePie = editorPlaySessionActive_ && editorPlayUsesSceneRecreate_;
#endif
		if (scene_)
		{
#ifdef USE_IMGUI
			if (continueSceneRecreatePie)
			{
				scene_->EndEditorPlay(); // Runtime Scene遷移前に現在SceneのPIE後処理だけを実行する。
			}
			else if (editorPlaySessionActive_)
			{
				scene_->EndEditorPlay();
				K4E::EditorPlaySessionManager::GetInstance()->CancelSessionWithoutRestore();
				editorPlaySessionActive_ = false;
				editorPlayUsesSceneRecreate_ = false;
				editorPlayOriginSceneId_.clear();
				K4E::EditorPlayController::GetInstance()->CommitStopped();
			}
#endif
			scene_->Finalize();
		}
		scene_ = std::move(nextScene_);
		currentSceneDefinition_ = std::move(nextSceneDefinition_);
		nextSceneDefinition_ = {};
		if (scene_)
		{
			scene_->SetSceneManager(this);
			scene_->Initialize();
			scene_->StartLoad();
#ifdef USE_IMGUI
			if (continueSceneRecreatePie)
			{
				scene_->BeginEditorPlay(); // Runtime中に遷移した新Sceneも同じPIE Sessionとして継続する。
			}
			K4E::EditorWindowManager::GetInstance()->AddOutputLog(
				K4E::EditorLogLevel::Info,
				"Scene開始: " + currentSceneDefinition_.id + " [Class=" + currentSceneDefinition_.className + "]");
#endif
		}
	}
} // namespace Ken4lowEngine
