#pragma once

#include "ReflectionProbeManager.h"
#include "PlanarReflectionSceneBridge.h"
#include "ActorWorld.h"
#include "ModelComponent.h"
#include "PlanarReflectionComponent.h"
#include "ReflectionProbeComponent.h"
#include <Engine/Graphics/Renderer/GpuFluid/Sph/Interaction/GpuSphRigidbodyInteraction.h>

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#endif

namespace Ken4lowEngine
{
	/// <summary>ActorWorldをReflection Capture群の登録/Captureへ接続するBridgeです。</summary>
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
			// W9はMain Scene描画直前の最新Physics Transformを使い、SPH粒子とRigidbodyを双方向に接続する。
			GpuSphRigidbodyInteraction::GetInstance()->Update(actorWorld);

			SyncProbes(actorWorld);
			actorWorld.PrepareRenderState(); // Probe内のDirect Lightingも現在SceneのLightComponent値を使う。
			const bool probeCaptured = ReflectionProbeManager::GetInstance()->CapturePending(
				[&actorWorld]()
				{
					DrawStaticScene(actorWorld);
				});
			const bool planarCaptured = PlanarReflectionSceneBridge::CapturePending(actorWorld); // Main SceneTargetをBindする前の既存Reflection hookをPlanarにも共有する。
			return probeCaptured || planarCaptured;
		}

		static void DrawStaticScene(ActorWorld& actorWorld)
		{
			for (const auto& actor : actorWorld.GetActors())
			{
				if (!CanUseActor(actor.get()) || HasActivePlanarSurface(actor.get())) continue;
				for (ModelComponent* model : actor->GetComponents<ModelComponent>())
				{
					if (model)
					{
						model->DrawReflectionCapture(); // v1は静的Opaque/MaskedだけをCaptureし、透明/Particle/鏡面は再帰させない。
					}
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
