#pragma once

#include "PlanarReflectionManager.h"
#include "ReflectionCaptureDrawable.h"
#include "ActorWorld.h"
#include "CameraManager.h"
#include "PlanarReflectionComponent.h"

#include <algorithm>
#include <cstdint>
#include <vector>

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
			struct CaptureItem
			{
				ReflectionCaptureDrawable* drawable = nullptr;
				MaterialBlendMode blendMode = MaterialBlendMode::Opaque;
				float sortDepth = 0.0f;
				uint64_t submissionOrder = 0;
			};

			CameraManager* cameraManager = CameraManager::GetInstance();
			const Vector3 cameraPosition = cameraManager->GetActiveCameraPosition();
			const Vector3 cameraForward = Vector3::NormalizeSafe(cameraManager->GetActiveCameraForward(), { 0.0f, 0.0f, 1.0f });
			std::vector<CaptureItem> captureItems;
			uint64_t submissionOrder = 0;

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

					const Vector3 sortPosition = drawable->GetReflectionCaptureSortPosition();
					const Vector3 toDrawable = sortPosition - cameraPosition;
					CaptureItem item{};
					item.drawable = drawable;
					item.blendMode = drawable->GetReflectionCaptureBlendMode();
					item.sortDepth = Vector3::Dot(toDrawable, cameraForward);
					item.submissionOrder = submissionOrder++;
					captureItems.push_back(item);
				}
			}

			std::stable_sort(
				captureItems.begin(),
				captureItems.end(),
				[](const CaptureItem& lhs, const CaptureItem& rhs)
				{
					const int lhsPass = GetCapturePassOrder(lhs.blendMode);
					const int rhsPass = GetCapturePassOrder(rhs.blendMode);
					if (lhsPass != rhsPass) return lhsPass < rhsPass;

					const bool transparentPass =
						lhs.blendMode == MaterialBlendMode::Transparent || lhs.blendMode == MaterialBlendMode::Additive;
					if (lhs.sortDepth != rhs.sortDepth)
					{
						return transparentPass ? lhs.sortDepth > rhs.sortDepth : lhs.sortDepth < rhs.sortDepth;
					}
					return lhs.submissionOrder < rhs.submissionOrder;
				});

			for (const CaptureItem& item : captureItems)
			{
				if (item.drawable) item.drawable->DrawReflectionCapture(); // Opaque→Masked→Transparent→Additive順で描き、透明系は反射Camera基準Back-to-Frontにする。
			}
		}

	private:
		static int GetCapturePassOrder(MaterialBlendMode blendMode)
		{
			switch (blendMode)
			{
			case MaterialBlendMode::Opaque: return 0;
			case MaterialBlendMode::Masked: return 1;
			case MaterialBlendMode::Transparent: return 2;
			case MaterialBlendMode::Additive: return 3;
			default: return 0;
			}
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
