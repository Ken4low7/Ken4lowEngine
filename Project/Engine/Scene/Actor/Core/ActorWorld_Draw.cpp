#include "ActorWorld.h"

#include "AnimatedModelComponent.h"
#include "ColliderComponent.h"
#include "GaugeComponent.h"
#include "InstancedModelComponent.h"
#include "LightComponent.h"
#include "ModelComponent.h"
#include "SkeletalMeshComponent.h"
#include "SpriteComponent.h"
#include "TextComponent.h"
#include "WorldGaugeComponent.h"
#include "WorldSpriteComponent.h"
#include "WorldTextComponent.h"

#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"
#include "Engine/Graphics/Renderer/GpuFluid/Manager/GpuFluidManager.h"
#include "Engine/Graphics/Renderer/GpuFluid/Volumetric/Manager/GpuVolumetricFluidManager.h"
#include "Engine/Graphics/Renderer/GpuParticle/Renderer/GpuParticleForwardRenderBridge.h"
#include "LightManager.h"
#include "SceneComponent.h"
#include "SpriteManager.h"
#include <GameTimer.h>

#ifdef USE_IMGUI
#include <Editor/EditorActorStateRegistry.h>
#include <Editor/EditorModeController.h>
#include <Editor/EditorPlayController.h>
#endif

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Ken4lowEngine
{
	namespace
	{
		bool IsHiddenByEditorOutliner(const Actor& actor)
		{
#ifdef USE_IMGUI
			const bool isEditorEditing = EditorModeController::GetInstance()->IsEditorModeEnabled() &&
				EditorPlayController::GetInstance()->IsEditing();
			return isEditorEditing && !EditorActorStateRegistry::GetInstance()->IsVisible(&actor);
#else
			(void)actor;
			return false;
#endif
		}
	}

	void ActorWorld::Draw()
	{
		SyncLightComponentsToLightManager(); // 描画直前のLightComponent設定をLightManagerへ渡す

		ForwardRenderQueue* forwardQueue = ForwardRenderQueue::GetInstance();
		forwardQueue->BeginFrame();

		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive() || IsHiddenByEditorOutliner(*actor))
			{
				continue;
			}

			const auto modelComponents = actor->GetComponents<ModelComponent>();
			for (ModelComponent* modelComponent : modelComponents)
			{
				if (modelComponent)
				{
					modelComponent->SubmitForwardOpaque(*forwardQueue);
					modelComponent->SubmitForwardMasked(*forwardQueue); // MaskedもDepthWriteするためOpaque直後の専用Bucketへ収集する。
					modelComponent->SubmitForwardTransparent(*forwardQueue);
					modelComponent->SubmitForwardAdditive(*forwardQueue);
				}
			}

			const auto instancedModelComponents = actor->GetComponents<InstancedModelComponent>();
			for (InstancedModelComponent* instancedModelComponent : instancedModelComponents)
			{
				if (instancedModelComponent)
				{
					instancedModelComponent->SubmitForwardOpaque(*forwardQueue);
					instancedModelComponent->SubmitForwardMasked(*forwardQueue);
					instancedModelComponent->SubmitForwardTransparent(*forwardQueue);
					instancedModelComponent->SubmitForwardAdditive(*forwardQueue); // Instancingも全Material分類をStatic Modelと同じBucket契約へ揃える。
				}
			}

			const auto animatedModelComponents = actor->GetComponents<AnimatedModelComponent>();
			for (AnimatedModelComponent* animatedModelComponent : animatedModelComponents)
			{
				if (animatedModelComponent)
				{
					animatedModelComponent->SubmitForwardOpaque(*forwardQueue);
					animatedModelComponent->SubmitForwardMasked(*forwardQueue);
					animatedModelComponent->SubmitForwardTransparent(*forwardQueue);
					animatedModelComponent->SubmitForwardAdditive(*forwardQueue);
				}
			}

			const auto skeletalMeshComponents = actor->GetComponents<SkeletalMeshComponent>();
			for (SkeletalMeshComponent* skeletalMeshComponent : skeletalMeshComponents)
			{
				if (skeletalMeshComponent)
				{
					skeletalMeshComponent->SubmitForwardOpaque(*forwardQueue);
					skeletalMeshComponent->SubmitForwardMasked(*forwardQueue);
					skeletalMeshComponent->SubmitForwardTransparent(*forwardQueue);
					skeletalMeshComponent->SubmitForwardAdditive(*forwardQueue); // Node/Skeletal双方を同じ4Bucket契約へ収集する。
				}
			}
		}

		GpuParticleForwardRenderBridge::GetInstance()->Submit(*forwardQueue); // GPU Particleは粒子単位ではなくSystem Packetとして透明系Bucketへ接続する。

		GpuFluidManager* gpuFluidManager = GpuFluidManager::GetInstance();
		// Actor更新・Physics補正後の最新Emitter/ColliderでComputeを記録し、同じFrameのTransparent描画へ渡す。
		gpuFluidManager->UpdateFromWorld(*this, GameTimer::GetInstance()->GetDeltaTime());
		gpuFluidManager->SubmitForward(*forwardQueue);

		GpuVolumetricFluidManager* volumetricFluidManager = GpuVolumetricFluidManager::GetInstance();
		// 3D Solverも同じWorld snapshotを使うが、Runtime既定OFFなのでPhase16 Sceneの描画と負荷は明示Enableまで変えない。
		volumetricFluidManager->UpdateFromWorld(*this, GameTimer::GetInstance()->GetDeltaTime());
		volumetricFluidManager->SubmitForward(*forwardQueue);

		forwardQueue->ExecuteBucket(ForwardRenderBucket::Opaque);
		forwardQueue->ExecuteBucket(ForwardRenderBucket::Masked);

		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive() || IsHiddenByEditorOutliner(*actor))
			{
				continue; // 無効またはEditorで非表示のActorは通常描画対象から外す
			}

			// Queue未移行Componentと派生Actor独自描画は従来経路を維持する。Queue所有ComponentはActor::Draw側で二重描画を抑止する。
			actor->Draw();
		}

		forwardQueue->ExecuteBucket(ForwardRenderBucket::Transparent); // 半透明はDepthを書き終えた不透明・旧経路3D描画の後にBackToFrontで合成する。
		forwardQueue->ExecuteBucket(ForwardRenderBucket::Additive); // 発光系は通常Alpha合成の後段へ分離し、加算Surface同士を安定順序で処理する。
		forwardQueue->EndFrame();
	}

	void ActorWorld::PrepareRenderState()
	{
		SyncLightComponentsToLightManager(); // Scene側でLightComponentを再走査せずActorWorldへ統一する。
	}

	void ActorWorld::DrawScreenSpaceUI()
	{
		struct ScreenSpaceUIDrawEntry
		{
			ActorComponent* component = nullptr;
			int drawOrder = 0;
			void (*draw)(ActorComponent*) = nullptr;
		};

		std::vector<ScreenSpaceUIDrawEntry> uiComponents;

		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive() || IsHiddenByEditorOutliner(*actor))
			{
				continue; // 削除予定、無効、Editor非表示のActorはUI描画対象から外す
			}

			const auto components = actor->GetComponents<SpriteComponent>();
			for (SpriteComponent* spriteComponent : components)
			{
				if (!spriteComponent || !spriteComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なSpriteComponentは描画しない
				}

				uiComponents.push_back({
					spriteComponent,
					spriteComponent->GetDrawOrder(),
					[](ActorComponent* component)
					{
						static_cast<SpriteComponent*>(component)->DrawScreenSpace();
					}
				});
			}

			const auto worldSpriteComponents = actor->GetComponents<WorldSpriteComponent>();
			for (WorldSpriteComponent* worldSpriteComponent : worldSpriteComponents)
			{
				if (!worldSpriteComponent || !worldSpriteComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なWorldSpriteComponentは描画しない
				}

				uiComponents.push_back({
					worldSpriteComponent,
					worldSpriteComponent->GetDrawOrder(),
					[](ActorComponent* component)
					{
						static_cast<WorldSpriteComponent*>(component)->DrawScreenSpace();
					}
				});
			}

			const auto textComponents = actor->GetComponents<TextComponent>();
			for (TextComponent* textComponent : textComponents)
			{
				if (!textComponent || !textComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なTextComponentは描画しない
				}

				uiComponents.push_back({
					textComponent,
					textComponent->GetDrawOrder(),
					[](ActorComponent* component)
					{
						static_cast<TextComponent*>(component)->DrawScreenSpace();
					}
				});
			}

			const auto worldTextComponents = actor->GetComponents<WorldTextComponent>();
			for (WorldTextComponent* worldTextComponent : worldTextComponents)
			{
				if (!worldTextComponent || !worldTextComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なWorldTextComponentは描画しない
				}

				uiComponents.push_back({
					worldTextComponent,
					worldTextComponent->GetDrawOrder(),
					[](ActorComponent* component)
					{
						static_cast<WorldTextComponent*>(component)->DrawScreenSpace();
					}
				});
			}

			const auto gaugeComponents = actor->GetComponents<GaugeComponent>();
			for (GaugeComponent* gaugeComponent : gaugeComponents)
			{
				if (!gaugeComponent || !gaugeComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なGaugeComponentは描画しない
				}

				uiComponents.push_back({
					gaugeComponent,
					gaugeComponent->GetDrawOrder(),
					[](ActorComponent* component)
					{
						static_cast<GaugeComponent*>(component)->DrawScreenSpace();
					}
				});
			}

			const auto worldGaugeComponents = actor->GetComponents<WorldGaugeComponent>();
			for (WorldGaugeComponent* worldGaugeComponent : worldGaugeComponents)
			{
				if (!worldGaugeComponent || !worldGaugeComponent->CanDrawScreenSpace())
				{
					continue; // 非表示または無効なWorldGaugeComponentは描画しない
				}

				uiComponents.push_back({
					worldGaugeComponent,
					worldGaugeComponent->GetDrawOrder(),
					[](ActorComponent* component)
					{
						static_cast<WorldGaugeComponent*>(component)->DrawScreenSpace();
					}
				});
			}
		}

		std::stable_sort(uiComponents.begin(), uiComponents.end(),
			[](const ScreenSpaceUIDrawEntry& a, const ScreenSpaceUIDrawEntry& b)
			{
				return a.drawOrder < b.drawOrder; // DrawOrderが小さいUI Componentから先に描画する
			});

		if (uiComponents.empty())
		{
			return; // 描画対象のUI Componentが無い場合は何もしない
		}

		SpriteManager::GetInstance()->SetRenderSetting_UI();

		for (const ScreenSpaceUIDrawEntry& entry : uiComponents)
		{
			if (entry.component && entry.draw)
			{
				entry.draw(entry.component);
			}
		}
	}

	void ActorWorld::DrawScreenSpaceSprites()
	{
		DrawScreenSpaceUI();
	}

	void ActorWorld::DrawShadow()
	{
		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive() || IsHiddenByEditorOutliner(*actor))
			{
				continue; // 無効またはEditorで非表示のActorはShadow描画対象から外す
			}

			// 影を落とすActorだけが内部Component経由でShadow描画される
			actor->DrawShadow();
		}
	}

	void ActorWorld::SyncLightComponentsToLightManager()
	{
		std::vector<LightManager::PunctualLightGPU> componentLights;

		for (const auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive() || IsHiddenByEditorOutliner(*actor))
			{
				continue; // 削除予定、無効、Editor非表示のActorはライト反映対象から外す
			}

			const auto lightComponents = actor->GetComponents<LightComponent>();
			for (const LightComponent* lightComponent : lightComponents)
			{
				if (!lightComponent || !lightComponent->IsActiveInHierarchy() || !lightComponent->IsEnabled() ||
					lightComponent->GetLightType() == LightComponent::LightType::None)
				{
					continue; // 無効なLightComponentは描画用ライトに登録しない
				}

				const Vector3& color = lightComponent->GetColor();

				LightManager::PunctualLightGPU light{};
				light.lightType = lightComponent->GetLightTypeValue();
				light.color = { color.x, color.y, color.z, 1.0f };
				light.intensity = lightComponent->GetIntensity();
				light.position = lightComponent->GetWorldPosition();
				light.radius = lightComponent->GetRange();
				light.decay = lightComponent->GetDecay();
				light.direction = lightComponent->CalculateDirection();
				light.distance = lightComponent->GetRange();
				const float outerAngle = std::clamp(lightComponent->GetOuterAngle(), 0.1f, 179.0f);
				const float innerAngle = std::clamp(lightComponent->GetInnerAngle(), 0.0f, outerAngle);
				light.cosAngle = std::cos(outerAngle * std::numbers::pi_v<float> / 180.0f);
				light.cosFalloffStart = std::cos(innerAngle * std::numbers::pi_v<float> / 180.0f);
				light.areaSize = lightComponent->GetAreaSize();
				light.enabled = 1u;

				componentLights.push_back(light);
			}
		}

		LightManager::GetInstance()->SetLightComponentLights(componentLights);
	}

} // namespace Ken4lowEngine
