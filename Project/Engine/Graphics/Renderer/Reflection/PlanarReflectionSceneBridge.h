#pragma once

#include "PlanarReflectionManager.h"
#include "ActorWorld.h"
#include "ModelComponent.h"
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
			const PlanarReflectionComponent* receiverSurface =
				excludedReceiver ? excludedReceiver->GetComponent<PlanarReflectionComponent>() : nullptr;

			for (const auto& actor : actorWorld.GetActors())
			{
				Actor* sceneActor = actor.get();
				if (!CanUseActor(sceneActor) || sceneActor == excludedReceiver) continue;
				if (HasActivePlanarSurface(sceneActor))
				{
					continue; // v1は鏡の中へ別の鏡を描かず、相互再帰や前Frame Texture依存を避ける。
				}

				for (ModelComponent* model : sceneActor->GetComponents<ModelComponent>())
				{
					if (!model || IsFullyBehindMirrorPlane(*model, receiverSurface)) continue;
					model->DrawReflectionCapture(); // 鏡の表側にあるOpaque/Maskedだけを反射Cameraから再描画する。
				}
			}
		}

	private:
		static bool IsFullyBehindMirrorPlane(
			const ModelComponent& model,
			const PlanarReflectionComponent* receiverSurface)
		{
			if (!receiverSurface || !model.HasReflectionCaptureBounds()) return false;
			const BoundingSphere bounds = model.GetReflectionCaptureBounds();
			const Vector3 planeNormal = receiverSurface->GetPlaneNormal();
			const float signedDistance = Vector3::Dot(bounds.center - receiverSurface->GetWorldPosition(), planeNormal);
			return signedDistance + bounds.radius < -0.001f; // 完全に鏡の裏側へ入った床/壁だけを簡易Clipし、鏡像を塞がないようにする。
		}

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
