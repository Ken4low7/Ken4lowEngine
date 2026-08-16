#define NOMINMAX
#include "GameApplication.h"
#include "SceneFactory.h"
#include "ParameterManager.h"
#include <Wireframe.h>
#include <DirectXCommon.h>
#include "Object3DCommon.h"
#include "PostEffectManager.h"
#include <SceneManager.h>
#include <BaseScene.h>
#include <Input.h>
#include <GameTimer.h>
#include "Engine/Graphics/Renderer/Reflection/ReflectionProbeManager.h"
#include "Engine/Graphics/Renderer/Reflection/ReflectionProbeSceneBridge.h"
#include "Engine/Vfx/Runtime/VfxCueRuntime.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"
#include "Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h"

#ifdef USE_IMGUI
#include <ImGuiManager.h>
#include "Editor/EditorGpuPickingManager.h"
#include "Editor/EditorSelectionOutlineManager.h"
#include "Editor/EditorShell.h"
#include "Editor/EditorWindowManager.h"
#include "Editor/EditorModeController.h"
#include "Engine/Vfx/Editor/VfxTimelineEditor.h"
#include "Engine/Vfx/Graph/Editor/VfxGraphEditor.h"
#include "Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h"
#endif // USE_IMGUI
#include "JsonAssets/JsonEditorWindow.h"
#include <DisplaySettings.h>
#include <WinApp.h>

namespace Ken4lowEngine
{
	namespace
	{
		class GpuSafeSceneTransition final : public ISceneTransition
		{
		public:
			explicit GpuSafeSceneTransition(DirectXCommon* dxCommon)
				: dxCommon_(dxCommon)
			{
			}

			void Initialize() override
			{
				busy_ = false;
				fullyCovered_ = false;
			}

			void Update(float deltaTime) override
			{
				(void)deltaTime;
				if (!busy_) return;

				if (dxCommon_)
				{
					DX12CommandManager* commandManager = dxCommon_->GetCommandManager();
					DX12FenceManager* fenceManager = dxCommon_->GetFenceManager();
					if (commandManager && fenceManager && commandManager->GetCommandQueue())
					{
						const UINT64 fenceValue = fenceManager->SignalAndGetValue(commandManager->GetCommandQueue());
						fenceManager->WaitForValue(fenceValue);
					}
				}

				fullyCovered_ = true; // Scene破棄直前のUpdateでも送信済みGPU workを完了させ、旧Scene Resourceの寿命を保証する。
			}

			void Draw2DSprites() override {}
			void DrawImGui() override {}
			void DrawInspectorContent() override {}

			void Finalize() override
			{
				busy_ = false;
				fullyCovered_ = false;
			}

			void StartCover() override
			{
				busy_ = true;
				fullyCovered_ = false; // Draw中のScene変更要求では即Swapせず、次のUpdate境界まで延期する。
			}

			void StartCrack() override
			{
				busy_ = false;
				fullyCovered_ = false;
			}

			bool IsFullyCovered() const override { return fullyCovered_; }
			bool IsBusy() const override { return busy_; }

		private:
			DirectXCommon* dxCommon_ = nullptr;
			bool busy_ = false;
			bool fullyCovered_ = false;
		};
	}

	GameApplication::GameApplication() = default;

	GameApplication::~GameApplication() = default;

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　初期化処理
	/// -------------------------------------------------------------
	void GameApplication::Initialize()
	{
		Framework::Initialize();
		ReflectionProbeManager::GetInstance()->Initialize(dxCommon_);
		renderPipelineController_.Initialize(dxCommon_);
		VfxCueRuntime::GetInstance()->Initialize(); // Cue Runtimeは既存Subsystem初期化後にFacadeだけ起動し、GPU Resourceは各Adapterへ委譲する。

#ifdef USE_IMGUI
		EditorModeController::GetInstance()->Initialize();
		EditorGpuPickingManager::GetInstance()->Initialize();
		EditorSelectionOutlineManager::GetInstance()->Initialize();
		VfxTimelineEditor::GetInstance()->Initialize();
		VfxGraphEditor::GetInstance()->Initialize(); // Phase25 Graph authoring/preview shares the existing GPU particle runtime.
#endif // USE_IMGUI

		Input::GetInstance()->Initialize(winApp_);
		ParameterManager::GetInstance()->LoadFiles();
		JsonEditorWindow::GetInstance()->Initialize();

		sceneManager_ = std::make_unique<SceneManager>();
		sceneManager_->Initialize();

#ifdef USE_IMGUI
		EditorWindowManager::GetInstance()->SetSceneManager(sceneManager_.get());
#endif // USE_IMGUI

		auto sceneFactory = std::make_unique<SceneFactory>();
		sceneManager_->SetAbstractSceneFactory(std::move(sceneFactory));

#ifdef _DEBUG
		const std::string startSceneName = "DebugScene";
#else
		const std::string startSceneName = "TitleScene";
#endif
		sceneManager_->ChangeScene(startSceneName);

		auto safeSceneTransition = std::make_unique<GpuSafeSceneTransition>(dxCommon_);
		safeSceneTransition->Initialize();
		sceneManager_->SetSceneTransition(std::move(safeSceneTransition));
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　更新処理
	/// -------------------------------------------------------------
	void GameApplication::Update()
	{
		GameTimer::GetInstance()->BeginFrame();
		GameTimer::GetInstance()->BeginUpdate();
		Input::GetInstance()->Update();

#ifdef USE_IMGUI
		EditorModeController::GetInstance()->Update(Input::GetInstance());
		sceneManager_->ProcessEditorPlayRequests();
#endif // USE_IMGUI

		// Cameraの通常更新前に前FrameのVFX presentation offsetだけ戻し、shakeをTransformへ累積しない。
		VfxCueRuntime::GetInstance()->BeginFrame();
		VfxGraphRuntime::GetInstance()->BeginFrame();

		if (defaultCamera_)
		{
			defaultCamera_->Update();
		}

		Framework::Update();
		sceneManager_->Update();

		ActorWorld* actorWorld = nullptr;
		if (BaseScene* currentScene = sceneManager_->GetCurrentScene())
		{
			actorWorld = currentScene->GetSceneActorWorld();
		}
		VfxGraphRuntime::GetInstance()->UpdateScalability();
		VfxCueRuntime::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime(), actorWorld);

		PostEffectManager::GetInstance()->Update();
		JsonEditorWindow::GetInstance()->Update(GameTimer::GetInstance()->GetDeltaTime());
		GameTimer::GetInstance()->EndFrame();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　描画処理
	/// -------------------------------------------------------------
	void GameApplication::Draw()
	{
		GameTimer::GetInstance()->BeginDraw();

		RenderPipelineController::FrameCallbacks callbacks{};
		callbacks.prepareShadowPass = [this]()
			{
				sceneManager_->PrepareShadowPass();
			};
		callbacks.drawShadowObjects = [this]()
			{
				sceneManager_->DrawShadowObjects();
			};
		callbacks.drawGameWorldToSceneTarget = [this]()
			{
				DrawGameWorldToSceneTarget();
			};
		callbacks.renderPostEffectToGameRenderTarget = []()
			{
				PostEffectManager::GetInstance()->RenderPostEffect();
			};
		callbacks.beginGameRenderTargetOverlay = []()
			{
				PostEffectManager::GetInstance()->BeginGameRenderTargetOverlay();
			};
		callbacks.drawScene2DOverlay = [this]()
			{
				DrawCurrentScene2DOverlay();
			};
		callbacks.endGameRenderTargetOverlay = []()
			{
				PostEffectManager::GetInstance()->EndGameRenderTargetOverlay();
			};
		callbacks.applyPostEffectToBackBuffer = [this]()
			{
				ApplyPostEffectToBackBuffer();
			};
		callbacks.rebindBackBufferForGameOverlay = [this]()
			{
				dxCommon_->RebindBackBufferForGameOverlay();
			};
		callbacks.drawGameUIToBackBuffer = [this]()
			{
				DrawGameUIToBackBuffer();
			};

		bool editorModeEnabled = false;
#ifdef USE_IMGUI
		callbacks.drawImGuiOverlay = []()
			{
				ImGuiManager::GetInstance()->Draw();
			};
		editorModeEnabled = EditorModeController::GetInstance()->ShouldDrawEditorUi();
		if (editorModeEnabled)
		{
			callbacks.renderEditorSelectionOutline = [this]()
				{
					EditorSelectionOutlineManager::GetInstance()->Render(sceneManager_->GetCurrentScene());
				};
			callbacks.buildEditorUi = [this]()
				{
					ImGuiManager::GetInstance()->BeginFrame();

					auto* editorWindows = EditorWindowManager::GetInstance();
					editorWindows->Draw();
					auto& editorWindowState = editorWindows->GetWindowState();
					JsonEditorWindow::GetInstance()->Draw(&editorWindowState.showJsonAssetManager);
					VfxTimelineEditor::GetInstance()->Draw(&editorWindowState.showVfxTimeline);
					VfxGraphEditor::GetInstance()->Draw(&editorWindowState.showVfxGraphEditor);
					VfxDiagnosticsWindow::GetInstance()->Draw(editorWindowState.showVfxGraphEditor);

					winApp_->DrawDisplaySettingsImGui(&editorWindowState.showDisplay);
					ParameterManager::GetInstance()->Update(&editorWindowState.showParameters);
					Object3DCommon::GetInstance()->DrawImGui();
					sceneManager_->DrawImGui();
					PostEffectManager::GetInstance()->ImGuiRender(&editorWindowState.showPostEffectSettings);
					EditorShell::GetInstance()->DrawViewportOverlay();
					ImGuiManager::GetInstance()->EndFrame();
					(void)EditorWindowManager::GetInstance()->GetMainViewportSize();
				};
			callbacks.executeEditorPickingPass = [this]()
				{
					const EditorGpuPickingManager::ExecuteResult result =
						EditorGpuPickingManager::GetInstance()->Execute(sceneManager_->GetCurrentScene());
					if (result.executed && !result.message.empty())
					{
						EditorWindowManager::GetInstance()->AddOutputLog(
							result.hit ? EditorLogLevel::Info : EditorLogLevel::Info,
							result.message);
					}
				};
		}
#endif // USE_IMGUI

		renderPipelineController_.ExecuteFrame(editorModeEnabled, callbacks);
		GameTimer::GetInstance()->EndDraw();
		GameTimer::GetInstance()->BeginPresent();
		dxCommon_->EndDraw();
		GameTimer::GetInstance()->EndPresent();
		GameTimer::GetInstance()->EndFrame();
		VfxGraphDiagnostics::GetInstance()->CaptureFrame(); // Phase28 records counters after the same frame's timing is finalized, without a GPU fence wait.
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　　　　　終了処理
	/// -------------------------------------------------------------
	void GameApplication::Finalize()
	{
#ifdef USE_IMGUI
		EditorWindowManager::GetInstance()->SetSceneManager(nullptr);
		VfxTimelineEditor::GetInstance()->Finalize();
		VfxGraphEditor::GetInstance()->Finalize();
		EditorSelectionOutlineManager::GetInstance()->Finalize();
		EditorGpuPickingManager::GetInstance()->Finalize();
#endif // USE_IMGUI

		// VFX transient ActorをSceneがまだ生存しているうちに停止し、World/Component参照を残さない。
		VfxCueRuntime::GetInstance()->Finalize();

		{
			auto sceneManager = std::move(sceneManager_);
			if (sceneManager)
			{
				sceneManager->Finalize();
			}
		}

		ReflectionProbeManager::GetInstance()->Finalize();
		Framework::Finalize();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　ゲーム本編3D描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene3DPass()
	{
		Object3DCommon::GetInstance()->BeginObject3DPass();
		sceneManager_->Draw3DObjects();
		Object3DCommon::GetInstance()->EndObject3DPass();
		Wireframe::GetInstance()->Draw();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　HUD/UI/Sprite描画の共通処理
	/// -------------------------------------------------------------
	void GameApplication::DrawCurrentScene2DOverlay()
	{
		sceneManager_->Draw2DSprites();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　SceneRenderTargetへのゲーム本編描画処理
	/// -------------------------------------------------------------
	void GameApplication::DrawGameWorldToSceneTarget()
	{
		if (sceneManager_)
		{
			if (BaseScene* scene = sceneManager_->GetCurrentScene())
			{
				if (ActorWorld* actorWorld = scene->GetSceneActorWorld())
				{
					ReflectionProbeSceneBridge::CapturePending(*actorWorld);
				}
			}
		}

		PostEffectManager::GetInstance()->BeginDraw();
		DrawCurrentScene3DPass();
		PostEffectManager::GetInstance()->EndDraw();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　BackBufferへのポストエフェクト反映処理
	/// -------------------------------------------------------------
	void GameApplication::ApplyPostEffectToBackBuffer()
	{
		PostEffectManager::GetInstance()->RenderPostEffectToBackBuffer();
	}

	/// -------------------------------------------------------------
	///　　　　　　　　　BackBufferへのUI描画処理
	/// -------------------------------------------------------------
	void GameApplication::DrawGameUIToBackBuffer()
	{
		DrawCurrentScene2DOverlay();
	}

} // namespace Ken4lowEngine
