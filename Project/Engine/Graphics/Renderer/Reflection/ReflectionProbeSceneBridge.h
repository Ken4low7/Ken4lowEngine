#pragma once

#include "ReflectionProbeManager.h"
#include "ActorWorld.h"
#include "ModelComponent.h"
#include "ReflectionProbeComponent.h"

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>ActorWorldをReflection Probeの登録/Captureへ接続する薄いBridgeです。</summary>
	class ReflectionProbeSceneBridge
	{
	public:
		static void SyncProbes(ActorWorld& actorWorld)
		{
			for (const auto& actor : actorWorld.GetActors())
			{
				if (!CanUseActor(actor.get())) continue;
				for (ReflectionProbeComponent* probe : actor->GetComponents<ReflectionProbeComponent>())
				{
					if (probe && probe->IsActiveInHierarchy())
					{
						probe->SyncToManager(); // Editor Gizmo移動もCapture直前に最新World位置へ同期する。
					}
				}
			}
		}

		static bool CapturePending(ActorWorld& actorWorld)
		{
			SyncProbes(actorWorld);
			actorWorld.PrepareRenderState(); // Probe内のDirect Lightingも現在SceneのLightComponent値を使う。
			return ReflectionProbeManager::GetInstance()->CapturePending(
				[&actorWorld]()
				{
					DrawStaticScene(actorWorld);
				});
		}

		static void DrawStaticScene(ActorWorld& actorWorld)
		{
			for (const auto& actor : actorWorld.GetActors())
			{
				if (!CanUseActor(actor.get())) continue;
				for (ModelComponent* model : actor->GetComponents<ModelComponent>())
				{
					if (model)
					{
						model->DrawReflectionCapture(); // v1は静的Opaque/MaskedだけをCaptureし、透明/Particleは二重描画しない。
					}
				}
			}
		}

	private:
		static bool CanUseActor(const Actor* actor)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive()) return false;
#ifdef USE_IMGUI
			const bool isEditorEditing = EditorModeController::GetInstance()->IsEditorModeEnabled() &&
				EditorPlayController::GetInstance()->IsEditing();
			if (isEditorEditing && !EditorActorStateRegistry::GetInstance()->IsVisible(actor)) return false;
#endif
			return true;
		}
	};
} // namespace Ken4lowEngine
