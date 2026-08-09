#include "ActorWorld.h"

#include "ColliderComponent.h"
#include "GaugeComponent.h"
#include "LightComponent.h"
#include "SpriteComponent.h"
#include "TextComponent.h"
#include "WorldGaugeComponent.h"
#include "WorldSpriteComponent.h"
#include "WorldTextComponent.h"

#include "LightManager.h"
#include "SceneComponent.h"
#include "SpriteManager.h"

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

		for (auto& actor : actors_)
		{
			if (!actor || actor->IsPendingDestroy() || !actor->IsActive() || IsHiddenByEditorOutliner(*actor))
			{
				continue; // 無効またはEditorで非表示のActorは通常描画対象から外す
			}

			// 通常描画を持つActorだけが内部Component経由で描画される
			actor->Draw();
		}
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
