# Manager / Singleton宣言一覧

調査基準: master `d472d306`。外部ライブラリを除くEngine/Applicationのヘッダー宣言を集計。

GetInstance公開型は94件。返却型が自身のポインタ/参照である宣言を数え、同じファイルのforward declarationや単なる呼出は数えない。実行時生成数やthread safetyを保証する一覧ではない。所有Moduleは現行EngineModules.jsonによる分類であり、責務が適切との評価ではない。

各系統の起動/終了と所有は [構造調査](EngineArchitectureAudit.md) を参照。通常所有のManagerとstaticユーティリティは後半で分ける。

## GetInstance公開型

| Module | 型 | 宣言 |
| --- | --- | --- |
| Core | FrameMemory | [FrameMemory.h](../Project/Engine/Core/Memory/FrameMemory.h) |
| Core | GameTimer | [GameTimer.h](../Project/Engine/Core/Time/Core/GameTimer.h) |
| Core | JobSystem | [JobSystem.h](../Project/Engine/Core/Concurrency/JobSystem.h) |
| Core | ProjectSettings | [ProjectSettings.h](../Project/Engine/Core/Project/ProjectSettings.h) |
| Core | ReliabilityTelemetry | [ReliabilityTelemetry.h](../Project/Engine/Core/Diagnostics/ReliabilityTelemetry.h) |
| Core | ResolutionManager | [ResolutionManager.h](../Project/Engine/Platform/Windows/ResolutionManager.h) |
| Core | StreamingManager | [StreamingManager.h](../Project/Engine/Core/Streaming/StreamingManager.h) |
| Core | WinApp | [WinApp.h](../Project/Engine/Platform/Windows/WinApp.h) |
| Editor | EditorActorStateRegistry | [EditorActorStateRegistry.h](../Project/Engine/Editor/EditorActorStateRegistry.h) |
| Editor | EditorCommandHistory | [EditorCommandHistory.h](../Project/Engine/Editor/EditorCommandHistory.h) |
| Editor | EditorContentBrowserPanel | [EditorContentBrowserPanel.h](../Project/Engine/Editor/EditorContentBrowserPanel.h) |
| Editor | EditorContext | [EditorContext.h](../Project/Engine/Editor/EditorContext.h) |
| Editor | EditorDiagnosticsPanel | [EditorDiagnosticsPanel.h](../Project/Engine/Editor/EditorDiagnosticsPanel.h) |
| Editor | EditorGpuPickingManager | [EditorGpuPickingManager.h](../Project/Engine/Editor/EditorGpuPickingManager.h) |
| Editor | EditorHierarchyPanel | [EditorHierarchyPanel.h](../Project/Engine/Editor/EditorHierarchyPanel.h) |
| Editor | EditorLevelDeferredController | [EditorLevelDeferredController.h](../Project/Engine/Editor/EditorLevelDeferredController.h) |
| Editor | EditorLevelService | [EditorLevelService.h](../Project/Engine/Editor/EditorLevelService.h) |
| Editor | EditorModeController | [EditorModeController.h](../Project/Engine/Editor/EditorModeController.h) |
| Editor | EditorOutputLog | [EditorOutputLog.h](../Project/Engine/Editor/EditorOutputLog.h) |
| Editor | EditorPlayController | [EditorPlayController.h](../Project/Engine/Editor/EditorPlayController.h) |
| Editor | EditorPlaySessionManager | [EditorPlaySessionManager.h](../Project/Engine/Editor/EditorPlaySessionManager.h) |
| Editor | EditorProfilerPanel | [EditorProfilerPanel.h](../Project/Engine/Editor/EditorProfilerPanel.h) |
| Editor | EditorSceneDeferredController | [EditorSceneDeferredController.h](../Project/Engine/Editor/EditorSceneDeferredController.h) |
| Editor | EditorSelectionOutlineManager | [EditorSelectionOutlineManager.h](../Project/Engine/Editor/EditorSelectionOutlineManager.h) |
| Editor | EditorShell | [EditorShell.h](../Project/Engine/Editor/EditorShell.h) |
| Editor | EditorTransformGizmo | [EditorTransformGizmo.h](../Project/Engine/Editor/EditorTransformGizmo.h) |
| Editor | EditorViewportController | [EditorViewportController.h](../Project/Engine/Editor/EditorViewportController.h) |
| Editor | EditorWindowManager | [EditorWindowManager.h](../Project/Engine/Editor/EditorWindowManager.h) |
| Editor | GpuFluidDiagnosticsPanel | [GpuFluidDiagnosticsPanel.h](../Project/Engine/Editor/GpuFluidDiagnosticsPanel.h) |
| Editor | GpuSphAdvancedDiagnosticsPanel | [GpuSphAdvancedDiagnosticsPanel.h](../Project/Engine/Editor/GpuSphAdvancedDiagnosticsPanel.h) |
| Editor | GpuSphRigidbodyInteractionDiagnosticsPanel | [GpuSphRigidbodyInteractionDiagnosticsPanel.h](../Project/Engine/Editor/GpuSphRigidbodyInteractionDiagnosticsPanel.h) |
| Editor | GpuVolumetricFluidDiagnosticsPanel | [GpuVolumetricFluidDiagnosticsPanel.h](../Project/Engine/Editor/GpuVolumetricFluidDiagnosticsPanel.h) |
| Editor | VfxTimelineEditor | [VfxTimelineEditor.h](../Project/Engine/Vfx/Editor/VfxTimelineEditor.h) |
| Runtime | AnimationPipelineBuilder | [AnimationPipelineBuilder.h](../Project/Engine/Graphics/Renderer/Animation/Pipeline/AnimationPipelineBuilder.h) |
| Runtime | AssetRegistry | [AssetRegistry.h](../Project/Engine/Graphics/Resource/Asset/AssetRegistry.h) |
| Runtime | AssetSystem | [AssetSystem.h](../Project/Engine/Graphics/Resource/Asset/AssetSystem.h) |
| Runtime | AudioManager | [AudioManager.h](../Project/Engine/System/Audio/Manager/AudioManager.h) |
| Runtime | BladeTrailRenderer | [BladeTrailRenderer.h](../Project/Engine/Graphics/Renderer/BladeTrail/BladeTrailRenderer.h) |
| Runtime | BlendStateFactory | [BlendStateFactory.h](../Project/Engine/Graphics/RenderState/Blend/BlendStateFactory.h) |
| Runtime | CameraManager | [CameraManager.h](../Project/Engine/Graphics/Camera/Manager/CameraManager.h) |
| Runtime | CullingDiagnostics | [CullingDiagnostics.h](../Project/Engine/Graphics/Culling/CullingDiagnostics.h) |
| Runtime | DSVManager | [DSVManager.h](../Project/Engine/Graphics/Descriptor/DSV/DSVManager.h) |
| Runtime | DebugCamera | [DebugCamera.h](../Project/Engine/Graphics/Camera/DebugCamera/DebugCamera.h) |
| Runtime | DirectXCommon | [DirectXCommon.h](../Project/Engine/Graphics/Device/Facade/DirectXCommon.h) |
| Runtime | EffectSystem | [GpuParticleManager.h](../Project/Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.h) |
| Runtime | EnvironmentMapManager | [EnvironmentMapManager.h](../Project/Engine/Graphics/Renderer/Environment/EnvironmentMapManager.h) |
| Runtime | ForwardRenderQueue | [ForwardRenderQueue.h](../Project/Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h) |
| Runtime | FrameAllocationTracker | [FrameAllocationTracker.h](../Project/Engine/DebugTools/Performance/FrameAllocationTracker.h) |
| Runtime | GameplayAbilityDiagnostics | [GameplayAbilityDiagnostics.h](../Project/Engine/Gameplay/Diagnostics/GameplayAbilityDiagnostics.h) |
| Runtime | GameplayEventRouter | [GameplayEventRouter.h](../Project/Engine/Gameplay/Events/GameplayEventRouter.h) |
| Runtime | GpuDeferredReleaseQueue | [GpuDeferredReleaseQueue.h](../Project/Engine/Graphics/Resource/Asset/GpuDeferredReleaseQueue.h) |
| Runtime | GpuFluidForwardRenderBridge | [GpuFluidForwardRenderBridge.h](../Project/Engine/Graphics/Renderer/GpuFluid/Renderer/GpuFluidForwardRenderBridge.h) |
| Runtime | GpuFluidManager | [GpuFluidManager.h](../Project/Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h) |
| Runtime | GpuParticleEffectRuntime | [GpuParticleEffectRuntime.h](../Project/Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h) |
| Runtime | GpuParticleForwardRenderBridge | [GpuParticleForwardRenderBridge.h](../Project/Engine/Graphics/Renderer/GpuParticle/Renderer/GpuParticleForwardRenderBridge.h) |
| Runtime | GpuParticleManager | [GpuParticleManager.h](../Project/Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.h) |
| Runtime | GpuProductionLiquidManager | [GpuProductionLiquidManager.h](../Project/Engine/Graphics/Renderer/GpuFluid/Liquid/Manager/GpuProductionLiquidManager.h) |
| Runtime | GpuProductionLiquidOceanCoupler | [GpuProductionLiquidOceanCoupler.h](../Project/Engine/Graphics/Renderer/GpuFluid/Liquid/Manager/GpuProductionLiquidOceanCoupler.h) |
| Runtime | GpuProductionLiquidSecondaryClassifier | [GpuProductionLiquidSecondaryClassifier.h](../Project/Engine/Graphics/Renderer/GpuFluid/Liquid/Manager/GpuProductionLiquidSecondaryClassifier.h) |
| Runtime | GpuSphManager | [GpuSphManager.h](../Project/Engine/Graphics/Renderer/GpuFluid/Sph/Manager/GpuSphManager.h) |
| Runtime | GpuSphRigidbodyInteraction | [GpuSphRigidbodyInteraction.h](../Project/Engine/Graphics/Renderer/GpuFluid/Sph/Interaction/GpuSphRigidbodyInteraction.h) |
| Runtime | GpuSphScreenSpaceFluidRenderer | [GpuSphScreenSpaceFluidRenderer.h](../Project/Engine/Graphics/Renderer/GpuFluid/Sph/Renderer/GpuSphScreenSpaceFluidRenderer.h) |
| Runtime | GpuVolumetricFluidForwardRenderBridge | [GpuVolumetricFluidForwardRenderBridge.h](../Project/Engine/Graphics/Renderer/GpuFluid/Volumetric/Renderer/GpuVolumetricFluidForwardRenderBridge.h) |
| Runtime | GpuVolumetricFluidManager | [GpuVolumetricFluidManager.h](../Project/Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h) |
| Runtime | ImGuiManager | [ImGuiManager.h](../Project/Engine/DebugTools/ImGui/ImGuiManager.h) |
| Runtime | Input | [Input.h](../Project/Engine/System/Input/Input.h) |
| Runtime | JsonEditorWindow | [JsonEditorWindow.h](../Project/Engine/System/JsonAssets/JsonEditorWindow.h) |
| Runtime | LightManager | [LightManager.h](../Project/Engine/Graphics/Lighting/LightManager.h) |
| Runtime | MaterialRepository | [MaterialRepository.h](../Project/Engine/Graphics/Material/MaterialRepository.h) |
| Runtime | ModelManager | [ModelManager.h](../Project/Engine/Graphics/Resource/Model/ModelManager.h) |
| Runtime | Object3DCommon | [Object3DCommon.h](../Project/Engine/Graphics/Renderer/Object3D/Object3DCommon.h) |
| Runtime | ObjectIdPipeline | [ObjectIdPipeline.h](../Project/Engine/Graphics/Renderer/Object3D/ObjectIdPipeline.h) |
| Runtime | ParameterManager | [ParameterManager.h](../Project/Engine/System/Parameters/ParameterManager.h) |
| Runtime | PlanarReflectionCaptureDiagnostics | [PlanarReflectionCaptureDiagnostics.h](../Project/Engine/Graphics/Renderer/Reflection/PlanarReflectionCaptureDiagnostics.h) |
| Runtime | PlanarReflectionManager | [PlanarReflectionManager.h](../Project/Engine/Graphics/Renderer/Reflection/PlanarReflectionManager.h) |
| Runtime | PostEffectManager | [PostEffectManager.h](../Project/Engine/Graphics/PostEffect/Manager/PostEffectManager.h) |
| Runtime | PrefabInstanceRegistry | [PrefabInstanceRegistry.h](../Project/Engine/Scene/Actor/Serialization/PrefabInstanceRegistry.h) |
| Runtime | RTVManager | [RTVManager.h](../Project/Engine/Graphics/Descriptor/RTV/RTVManager.h) |
| Runtime | ReflectionProbeManager | [ReflectionProbeManager.h](../Project/Engine/Graphics/Renderer/Reflection/ReflectionProbeManager.h) |
| Runtime | RenderDepthContext | [RenderDepthContext.h](../Project/Engine/Graphics/RenderTarget/Depth/RenderDepthContext.h) |
| Runtime | RenderGraphVisualizer | [RenderGraphVisualizer.h](../Project/Engine/Graphics/RenderGraph/RenderGraphVisualizer.h) |
| Runtime | SRVManager | [SRVManager.h](../Project/Engine/Graphics/Descriptor/SRV/SRVManager.h) |
| Runtime | SkyBoxManager | [SkyBoxManager.h](../Project/Engine/Graphics/Renderer/SkyBox/SkyBoxManager.h) |
| Runtime | SpriteManager | [SpriteManager.h](../Project/Engine/Graphics/Renderer/Sprite/Core/SpriteManager.h) |
| Runtime | SubLevelManager | [SubLevelManager.h](../Project/Engine/Scene/Streaming/SubLevelManager.h) |
| Runtime | TextureManager | [TextureManager.h](../Project/Engine/Graphics/Resource/Texture/TextureManager.h) |
| Runtime | UAVManager | [UAVManager.h](../Project/Engine/Graphics/Descriptor/UAV/UAVManager.h) |
| Runtime | VfxCueRuntime | [VfxCueRuntime.h](../Project/Engine/Vfx/Runtime/VfxCueRuntime.h) |
| Runtime | VfxDiagnosticsWindow | [VfxDiagnosticsWindow.h](../Project/Engine/Vfx/Graph/Editor/VfxDiagnosticsWindow.h) |
| Runtime | VfxGraphDiagnostics | [VfxGraphDiagnostics.h](../Project/Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h) |
| Runtime | VfxGraphEditor | [VfxGraphEditor.h](../Project/Engine/Vfx/Graph/Editor/VfxGraphEditor.h) |
| Runtime | VfxGraphRuntime | [VfxGraphRuntime.h](../Project/Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h) |
| Runtime | Wireframe | [Wireframe.h](../Project/Engine/Graphics/Renderer/Wireframe/Core/Wireframe.h) |
| Runtime | WorldPartitionManager | [WorldPartitionManager.h](../Project/Engine/Scene/Streaming/WorldPartitionManager.h) |

## SingletonではないManager名の型

| 型 | 性質 / 所有者 | 宣言 |
| --- | --- | --- |
| CollisionManager | Legacy衝突登録/判定。Singletonではない | [CollisionManager.h](../Project/Engine/Physics/Collision/Legacy/CollisionManager.h) |
| DX12CommandManager | DirectXCommonがunique_ptrで所有 | [DX12CommandManager.h](../Project/Engine/Graphics/Device/Command/DX12CommandManager.h) |
| DX12FenceManager | DirectXCommonがunique_ptrで所有 | [DX12FenceManager.h](../Project/Engine/Graphics/Device/Synchronization/DX12FenceManager.h) |
| DXCCompilerManager | DirectXCommonがunique_ptrで所有 | [DXCCompilerManager.h](../Project/Engine/Graphics/Shader/Compiler/DXCCompilerManager.h) |
| JsonDataManager | static JSONファイル操作関数。グローバル資源所有者ではない | [JsonDataManager.h](../Project/Engine/System/JsonAssets/JsonDataManager.h) |
| LevelObjectManager | LevelのObject集合を通常所有する型 | [LevelObjectManager.h](../Project/Engine/Scene/Level/LevelObjectManager.h) |
| PostEffectRenderTargetManager | PostEffectManagerがunique_ptrで所有 | [PostEffectRenderTargetManager.h](../Project/Engine/Graphics/PostEffect/Manager/PostEffectRenderTargetManager.h) |
| ResourceManager | static resource生成関数。戻り値のComPtr所有は呼出元へ渡す | [ResourceManager.h](../Project/Engine/System/Resource/ResourceManager.h) |
| SceneManager | GameApplicationがunique_ptrで所有 | [SceneManager.h](../Project/Engine/Scene/Management/SceneManager.h) |
| StageChunkManager | Stageのchunk集合を管理する通常オブジェクト | [StageChunkManager.h](../Project/Engine/Scene/Level/StageChunkManager.h) |
| StageInstancingManager | Stageのinstancing描画を管理する通常オブジェクト | [StageInstancingManager.h](../Project/Engine/Scene/Level/StageInstancingManager.h) |

## 別形式の共有状態

- RenderPipelineController::activeController_ はGameApplication所有のcontrollerを借用するstatic pointer。所有Singletonではない。
- ActorFactory / ComponentFactory / SceneFactory は関数内staticの登録表を持つ。
- 数だけを減らす目的でサービスを統合しない。初期化/終了、借用参照、再初期化、スレッド利用を確認してから判断する。
