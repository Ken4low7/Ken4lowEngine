#pragma once

#include "PlanarReflectionManager.h"
#include "ReflectionCaptureDrawable.h"
#include "ActorWorld.h"
#include "PlanarReflectionComponent.h"

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>ActorWorld上のPlanar Reflection ComponentをCapture処理へ接続するBridgeです。</summary>
	class PlanarReflectionSceneBridge
	{
	public:
		static void SyncSurfaces(ActorWorld& actorWorld)
		{
			for (const auto& actor : actorWorld.GetActors())
			{
				if (!CanUseActor(actor.get())) continue;
				for (PlanarReflectionComponent* planar : actor->GetComponents<PlanarReflectionComponent>())
				{
					if (planar && planar->IsActiveInHierarchy())
					{
						planar->SyncToManager(); // Editor Gizmoで鏡面を移動/回転した直後のPlaneをCaptureへ反映する。
					}
				}
			}
		}

		static bool CapturePending(ActorWorld& actorWorld)
		{
			SyncSurfaces(actorWorld);
			actorWorld.PrepareRenderState();
			return PlanarReflectionManager::GetInstance()->CapturePending(
				[&actorWorld](const Actor* excludedReceiver)
				{
					DrawScene(actorWorld, excludedReceiver);
				});
		}

		static void DrawScene(ActorWorld& actorWorld, const Actor* excludedReceiver)
		{
			for (const auto& actor : actorWorld.GetActors())
			{
				Actor* sceneActor = actor.get();
				if (!CanUseActor(sceneActor) || sceneActor == excludedReceiver) continue;
				if (HasActivePlanarSurface(sceneActor))
				{
					continue; // 鏡の中へ別の鏡を描かず、相互再帰や前Frame Texture依存を避ける。
				}

				for (const auto& component : sceneActor->GetComponents())
				{
					if (!component || !component->IsActiveInHierarchy()) continue;
					auto* drawable = dynamic_cast<ReflectionCaptureDrawable*>(component.get());
					if (!drawable) continue;
					drawable->DrawReflectionCapture(); // 対応Componentを型追加なしで描き、鏡裏側の除去はReflection CameraのOblique Near Planeへ委ねる。
				}
			}
		}

	private:
		static bool HasActivePlanarSurface(Actor* actor)
		{
			if (!actor) return false;
			for (PlanarReflectionComponent* planar : actor->GetComponents<PlanarReflectionComponent>())
			{
				if (planar && planar->IsActiveInHierarchy() && planar->IsEnabled()) return true;
			}
			return false;
		}

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
