#pragma once

#include "InventoryComponent.h"
#include "PlayerInputComponent.h"
#include "PlayerMeleeAttackComponent.h"
#include "PlayerMovementComponent.h"
#include "WeaponComponent.h"

#include <Actor.h>
#include <ActorComponent.h>
#include <GameViewportConstants.h>
#include <GaugeComponent.h>
#include <Scene/Actor/Character/CharacterHealthComponent.h>
#include <SpriteComponent.h>
#include <TextComponent.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// PlayerのHP・武器・NoAmmo・HitMarker・動的Crosshairを同期するPresentation Component。
	class PlayerHudPresenterComponent final : public ActorComponent
	{
	public:
		void Update(float deltaTime) override
		{
			const float safeDeltaTime = (std::max)(0.0f, deltaTime);
			damageFlashRemaining_ = (std::max)(0.0f, damageFlashRemaining_ - safeDeltaTime);
			hitMarkerRemaining_ = (std::max)(0.0f, hitMarkerRemaining_ - safeDeltaTime);
			SyncHealth();
			SyncWeapon();
			SyncFeedbackText();
			ApplyCrosshairState(); // 表示はゲーム状態を読むだけにし、射撃・移動ロジックへ逆依存させない。
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("Player HUD Presentation");
			ImGui::Text("Visible: %s", hudVisible_ ? "Yes" : "No");
			ImGui::Text("Crosshair Targeted: %s", crosshairTargeted_ ? "Yes" : "No");
			ImGui::Text("Crosshair Spread: %.2f", currentCrosshairSpread_);
			ImGui::Text("Damage Flash: %.3f / Hit Marker: %.3f", damageFlashRemaining_, hitMarkerRemaining_);
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerHudPresenterComponent"; }

		void SetHudVisible(bool visible)
		{
			hudVisible_ = visible;
			if (GaugeComponent* gauge = FindNamedComponent<GaugeComponent>("Player HP Gauge")) gauge->SetVisible(visible);
			if (TextComponent* label = FindNamedComponent<TextComponent>("Player HP Label")) label->SetVisible(visible);
			if (TextComponent* ammo = FindNamedComponent<TextComponent>("Player Ammo Text")) ammo->SetVisible(visible);
			SyncFeedbackText();
			ApplyCrosshairState();
		}

		void SetCrosshairTargeted(bool targeted)
		{
			crosshairTargeted_ = targeted;
			ApplyCrosshairState();
		}

		void NotifyDamageTaken() { damageFlashRemaining_ = damageFlashDuration_; }

		void NotifyHit(bool killed)
		{
			hitMarkerRemaining_ = killed ? killMarkerDuration_ : hitMarkerDuration_;
			killMarkerActive_ = killed;
			SyncFeedbackText();
		}

		void ResetPresentation()
		{
			hudVisible_ = true;
			crosshairTargeted_ = false;
			damageFlashRemaining_ = 0.0f;
			hitMarkerRemaining_ = 0.0f;
			killMarkerActive_ = false;
			currentCrosshairSpread_ = baseCrosshairDistance_;
			SyncHealth();
			SyncWeapon();
			SyncFeedbackText();
			ApplyCrosshairState();
		}

	private:
		template<class T>
		T* FindNamedComponent(std::string_view name) const
		{
			Actor* owner = GetOwner();
			if (!owner) return nullptr;
			for (T* component : owner->GetComponents<T>())
			{
				if (component && component->GetName() == name) return component;
			}
			return nullptr;
		}

		void SyncHealth()
		{
			Actor* owner = GetOwner();
			CharacterHealthComponent* health = owner ? owner->GetComponent<CharacterHealthComponent>() : nullptr;
			if (!health) return;

			if (GaugeComponent* gauge = FindNamedComponent<GaugeComponent>("Player HP Gauge"))
			{
				gauge->SetMaxValue(health->GetMaxHealth());
				gauge->SetValue(health->GetCurrentHealth());
			}

			if (TextComponent* label = FindNamedComponent<TextComponent>("Player HP Label"))
			{
				char text[64]{};
				std::snprintf(text, sizeof(text), "PLAYER HP %.0f / %.0f", health->GetCurrentHealth(), health->GetMaxHealth());
				label->SetText(text);
				if (health->IsDead()) label->SetColor({ 1.0f, 0.2f, 0.2f, 1.0f });
				else if (damageFlashRemaining_ > 0.0f) label->SetColor({ 1.0f, 0.55f, 0.25f, 1.0f });
				else label->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		void SyncWeapon()
		{
			Actor* owner = GetOwner();
			WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			InventoryComponent* inventory = owner ? owner->GetComponent<InventoryComponent>() : nullptr;
			TextComponent* ammo = FindNamedComponent<TextComponent>("Player Ammo Text");
			if (!weapon || !ammo) return;

			char text[192]{};
			const int displaySlot = inventory ? inventory->GetSelectedSlot() + 1 : 1;
			const char* actionState = weapon->IsEquipAnimating() ? "   EQUIPPING" : (weapon->IsReloading() ? "   RELOADING" : "");
			if (weapon->IsMeleeWeapon())
			{
				std::snprintf(text, sizeof(text), "[%d] %s   MELEE%s", displaySlot, weapon->GetWeaponDisplayName(), actionState);
				ammo->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			}
			else
			{
				std::snprintf(text, sizeof(text), "[%d] %s   AMMO %d / %d   RESERVE %d%s   %s",
					displaySlot, weapon->GetWeaponDisplayName(),
					weapon->GetMagazineAmmo(), weapon->GetMagazineCapacity(), weapon->GetReserveAmmo(), actionState,
					weapon->IsAutomaticFireMode() ? "AUTO" : "SEMI");
				ammo->SetColor(weapon->GetMagazineAmmo() <= 0 ? Vector4{ 1.0f, 0.35f, 0.2f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		void SyncFeedbackText()
		{
			Actor* owner = GetOwner();
			WeaponComponent* weapon = owner ? owner->GetComponent<WeaponComponent>() : nullptr;
			if (TextComponent* noAmmo = FindNamedComponent<TextComponent>("Player No Ammo Text"))
			{
				noAmmo->SetVisible(hudVisible_ && weapon && weapon->ShouldShowNoAmmoFeedback());
			}
			if (TextComponent* marker = FindNamedComponent<TextComponent>("Player Hit Marker"))
			{
				marker->SetVisible(hudVisible_ && hitMarkerRemaining_ > 0.0f);
				marker->SetText(killMarkerActive_ ? "KILL" : "HIT");
				marker->SetColor(killMarkerActive_ ? Vector4{ 1.0f, 0.28f, 0.12f, 1.0f } : Vector4{ 1.0f, 1.0f, 1.0f, 1.0f });
			}
		}

		void ApplyCrosshairState()
		{
			Actor* owner = GetOwner();
			if (!owner) return;
			const PlayerInputComponent* input = owner->GetComponent<PlayerInputComponent>();
			const PlayerMovementComponent* movement = owner->GetComponent<PlayerMovementComponent>();
			const PlayerMeleeAttackComponent* melee = owner->GetComponent<PlayerMeleeAttackComponent>();
			const WeaponComponent* weapon = owner->GetComponent<WeaponComponent>();
			const bool aiming = input && input->IsAimHeld();
			const bool actionHidesCrosshair = (weapon && (weapon->IsReloading() || weapon->IsEquipAnimating())) || (melee && melee->IsAttacking());
			const bool visible = hudVisible_ && !actionHidesCrosshair;

			float spread = baseCrosshairDistance_;
			if (weapon) spread += weapon->GetReticleSpread() * weaponSpreadScale_;
			if (movement)
			{
				const float inputLength = std::sqrt(movement->GetMoveInputX() * movement->GetMoveInputX() + movement->GetMoveInputZ() * movement->GetMoveInputZ());
				spread += std::clamp(inputLength, 0.0f, 1.0f) * moveSpreadBonus_;
				if (movement->IsSprinting()) spread += sprintSpreadBonus_;
				if (!movement->IsGrounded() && !movement->IsInLadderArea()) spread += airSpreadBonus_;
			}
			if (aiming) spread *= adsSpreadMultiplier_;
			currentCrosshairSpread_ = std::clamp(spread, minimumCrosshairDistance_, maximumCrosshairDistance_);

			const Vector4 color = crosshairTargeted_
				? Vector4{ 1.0f, 0.25f, 0.25f, 1.0f }
				: Vector4{ 1.0f, 1.0f, 1.0f, aiming ? 0.55f : 0.92f };
			constexpr float centerX = static_cast<float>(GameViewportConstants::Width) * 0.5f;
			constexpr float centerY = static_cast<float>(GameViewportConstants::Height) * 0.5f;

			auto apply = [&](std::string_view name, const Vector2& position)
				{
					if (SpriteComponent* sprite = FindNamedComponent<SpriteComponent>(name))
					{
						sprite->SetVisible(visible);
						sprite->SetColor(color);
						sprite->SetPosition(position);
					}
				};
			apply("Player Crosshair Left", { centerX - currentCrosshairSpread_, centerY });
			apply("Player Crosshair Right", { centerX + currentCrosshairSpread_, centerY });
			apply("Player Crosshair Top", { centerX, centerY - currentCrosshairSpread_ });
			apply("Player Crosshair Bottom", { centerX, centerY + currentCrosshairSpread_ });
		}

	private:
		bool hudVisible_ = true;
		bool crosshairTargeted_ = false;
		bool killMarkerActive_ = false;
		float damageFlashRemaining_ = 0.0f;
		float damageFlashDuration_ = 0.18f;
		float hitMarkerRemaining_ = 0.0f;
		float hitMarkerDuration_ = 0.20f;
		float killMarkerDuration_ = 0.32f;
		float baseCrosshairDistance_ = 8.0f;
		float minimumCrosshairDistance_ = 5.0f;
		float maximumCrosshairDistance_ = 36.0f;
		float weaponSpreadScale_ = 2.2f;
		float moveSpreadBonus_ = 4.0f;
		float sprintSpreadBonus_ = 5.0f;
		float airSpreadBonus_ = 5.0f;
		float adsSpreadMultiplier_ = 0.58f;
		float currentCrosshairSpread_ = 8.0f;
	};
} // namespace Ken4lowEngine
