#pragma once

#include "InventoryComponent.h"
#include "PlayerCameraComponent.h"
#include "PlayerInputSnapshot.h"
#include "PlayerMeleeAttackComponent.h"
#include "PlayerMovementComponent.h"
#include "WeaponComponent.h"

#include <Actor.h>
#include <ActorComponent.h>

#include <algorithm>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Player入力を具体的なゲーム処理へ変換せず、対応する専用Componentへ要求だけを配送するComponent。
	class PlayerInputComponent final : public ActorComponent
	{
	public:
		void Update(float deltaTime) override
		{
			(void)deltaTime;
			Actor* owner = GetOwner();
			if (!owner)
			{
				ClearTransientRequests();
				return;
			}

			PlayerMovementComponent* movement = owner->GetComponent<PlayerMovementComponent>();
			PlayerCameraComponent* camera = owner->GetComponent<PlayerCameraComponent>();
			WeaponComponent* weapon = owner->GetComponent<WeaponComponent>();
			InventoryComponent* inventory = owner->GetComponent<InventoryComponent>();
			PlayerMeleeAttackComponent* melee = owner->GetComponent<PlayerMeleeAttackComponent>();

			if (movement)
			{
				movement->SetMoveInput(inputEnabled_ ? moveX_ : 0.0f, inputEnabled_ ? moveZ_ : 0.0f);
				movement->SetSprintHeld(inputEnabled_ && frameInput_.sprintHeld);
			}
			if (camera)
			{
				camera->SetAimHeld(inputEnabled_ && frameInput_.aimHeld && !(weapon && weapon->IsMeleeWeapon()));
				camera->SetSprintHeld(inputEnabled_ && frameInput_.sprintHeld && !frameInput_.aimHeld);
			}
			if (weapon) weapon->SetTriggerHeld(inputEnabled_ && frameInput_.fireHeld && !weapon->IsMeleeWeapon());

			const bool reloadAllowed = !restrictionsEnabled_ || allowReload_;
			if (inputEnabled_ && reloadAllowed && weapon && weapon->UsesAmmo() && !weapon->IsReloading() &&
				!weapon->IsEquipAnimating() && weapon->GetMagazineAmmo() <= 0 && weapon->GetReserveAmmo() > 0)
			{
				weapon->RequestReload(); // 弾倉が空になった次フレームに、予備弾があれば自動Reloadを要求する。
			}

			if (inputEnabled_)
			{
				if (jumpRequested_ && movement) movement->RequestJump();
				if (blinkRequested_ && movement) movement->RequestBlink();
				if (fireRequested_)
				{
					if (weapon && weapon->IsMeleeWeapon())
					{
						if (melee) melee->RequestAttack();
					}
					else if (weapon)
					{
						weapon->RequestFire();
					}
				}
				if (reloadRequested_ && weapon) weapon->RequestReload();
				if (toggleFireModeRequested_ && weapon) weapon->RequestToggleFireMode();
				if (meleeRequested_ && melee) melee->RequestAttack();
				if (inventorySlotRequested_ >= 0 && inventory) inventory->RequestSelectSlot(inventorySlotRequested_);
				if (weaponSwitchRequested_ != 0 && inventory) inventory->RequestCycle(weaponSwitchRequested_);
				if ((lookYawDelta_ != 0.0f || lookPitchDelta_ != 0.0f) && camera) camera->RequestLook(lookYawDelta_, lookPitchDelta_);
			}

			ClearTransientRequests(); // 左クリック近接と専用近接入力も同じ1フレーム要求として消費する。
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("プレイヤー入力要求");
			ImGui::Text("Enabled: %s", inputEnabled_ ? "ON" : "OFF");
			ImGui::Text("Move: %.2f, %.2f", moveX_, moveZ_);
			ImGui::Text("Sprint:%s Aim:%s Fire:%s Melee:%s Slot:%d Wheel:%d",
				frameInput_.sprintHeld ? "Yes" : "No", frameInput_.aimHeld ? "Yes" : "No",
				frameInput_.fireHeld ? "Yes" : "No", frameInput_.meleePressed ? "Yes" : "No",
				frameInput_.weaponSlotPressed, frameInput_.weaponSwitch);
			ImGui::Text("Restrictions: %s  Move:%s Shoot:%s Reload:%s WeaponSwitch:%s",
				restrictionsEnabled_ ? "ON" : "OFF", allowMove_ ? "ON" : "OFF", allowShoot_ ? "ON" : "OFF",
				allowReload_ ? "ON" : "OFF", weaponSwitchEnabled_ ? "ON" : "OFF");
			ImGui::SliderFloat("ADS感度倍率", &adsLookSensitivityMultiplier_, 0.1f, 1.0f, "%.2f");
#endif
		}

		std::string GetClassTypeName() const override { return "PlayerInputComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			ActorComponent::ToJson(outJson);
			outJson["InputEnabled"] = inputEnabled_;
			outJson["AdsLookSensitivityMultiplier"] = adsLookSensitivityMultiplier_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ActorComponent::FromJson(inJson);
			inputEnabled_ = inJson.value("InputEnabled", inputEnabled_);
			adsLookSensitivityMultiplier_ = std::clamp(inJson.value("AdsLookSensitivityMultiplier", adsLookSensitivityMultiplier_), 0.1f, 1.0f);
			ResetInputState();
		}

		void ApplyInputSnapshot(const ::InputSnapshot& snapshot, float mouseLookSensitivity, bool allowLookInput = true)
		{
			if (!inputEnabled_)
			{
				ResetInputState();
				return;
			}

			::InputSnapshot filtered = snapshot;
			ApplyRestrictions(filtered);
			frameInput_ = filtered;
			RequestMove(filtered.moveX, filtered.moveZ);

			if (allowLookInput)
			{
				const float sensitivityMultiplier = filtered.aimHeld ? adsLookSensitivityMultiplier_ : 1.0f;
				const float effectiveSensitivity = mouseLookSensitivity * sensitivityMultiplier;
				const float yawDelta = -filtered.lookMouseX * effectiveSensitivity;
				const float pitchDelta = filtered.lookMouseY * effectiveSensitivity;
				if (yawDelta != 0.0f || pitchDelta != 0.0f) RequestLook(yawDelta, pitchDelta);
			}

			if (filtered.jumpPressed) RequestJump();
			if (filtered.blinkPressed) RequestBlink();
			if (filtered.firePressed) RequestFire();
			if (filtered.reloadPressed) RequestReload();
			if (filtered.weaponSlotPressed > 0) RequestInventorySlot(filtered.weaponSlotPressed - 1);
			if (filtered.weaponSwitch != 0) RequestWeaponSwitch(filtered.weaponSwitch);
			if (filtered.toggleFireModePressed) RequestToggleFireMode();
			meleeRequested_ = filtered.meleePressed;
		}

		void SetInputRestrictions(bool enabled, bool allowMove, bool allowShoot, bool allowReload)
		{
			restrictionsEnabled_ = enabled;
			allowMove_ = allowMove;
			allowShoot_ = allowShoot;
			allowReload_ = allowReload;
		}

		void SetWeaponSwitchEnabled(bool enabled)
		{
			weaponSwitchEnabled_ = enabled;
			if (!enabled)
			{
				inventorySlotRequested_ = -1;
				weaponSwitchRequested_ = 0;
			}
		}

		void RequestMove(float x, float z) { moveX_ = std::clamp(x, -1.0f, 1.0f); moveZ_ = std::clamp(z, -1.0f, 1.0f); }
		void RequestLook(float yawDelta, float pitchDelta) { lookYawDelta_ += yawDelta; lookPitchDelta_ += pitchDelta; }
		void RequestJump() { jumpRequested_ = true; }
		void RequestBlink() { blinkRequested_ = true; }
		void RequestFire() { fireRequested_ = true; }
		void RequestReload() { reloadRequested_ = true; }
		void RequestInventorySlot(int slotIndex) { inventorySlotRequested_ = slotIndex; }
		void RequestWeaponSwitch(int direction) { weaponSwitchRequested_ = direction < 0 ? -1 : (direction > 0 ? 1 : 0); }
		void RequestToggleFireMode() { toggleFireModeRequested_ = true; }

		void ResetInputState()
		{
			moveX_ = 0.0f;
			moveZ_ = 0.0f;
			frameInput_ = {};
			ClearTransientRequests();
		}

		void SetInputEnabled(bool enabled)
		{
			inputEnabled_ = enabled;
			if (!enabled) ResetInputState();
		}

		bool IsInputEnabled() const { return inputEnabled_; }
		float GetMoveX() const { return moveX_; }
		float GetMoveZ() const { return moveZ_; }
		const ::InputSnapshot& GetFrameInput() const { return frameInput_; }
		bool IsSprintHeld() const { return frameInput_.sprintHeld; }
		bool IsAimHeld() const { return frameInput_.aimHeld; }
		bool IsFireHeld() const { return frameInput_.fireHeld; }
		bool IsBlinkRequested() const { return frameInput_.blinkPressed; }
		bool IsMeleeRequested() const { return meleeRequested_; }
		int GetWeaponSwitchRequested() const { return frameInput_.weaponSwitch; }
		bool IsToggleFireModeRequested() const { return frameInput_.toggleFireModePressed; }
		float GetAdsLookSensitivityMultiplier() const { return adsLookSensitivityMultiplier_; }
		bool IsWeaponSwitchEnabled() const { return weaponSwitchEnabled_; }

	private:
		void ApplyRestrictions(::InputSnapshot& input) const
		{
			if (!weaponSwitchEnabled_)
			{
				input.weaponSwitch = 0;
				input.weaponSlotPressed = 0;
			}
			if (!restrictionsEnabled_) return;
			input.weaponSwitch = 0;
			input.weaponSlotPressed = 0;
			input.toggleFireModePressed = false;
			input.meleePressed = false;
			if (!allowMove_)
			{
				input.moveX = 0.0f;
				input.moveZ = 0.0f;
				input.sprintHeld = false;
				input.jumpHeld = false;
				input.jumpPressed = false;
				input.blinkPressed = false;
			}
			if (!allowShoot_)
			{
				input.aimHeld = false;
				input.aimPressed = false;
				input.fireHeld = false;
				input.firePressed = false;
			}
			if (!allowReload_) input.reloadPressed = false;
		}

		void ClearTransientRequests()
		{
			lookYawDelta_ = 0.0f;
			lookPitchDelta_ = 0.0f;
			jumpRequested_ = false;
			blinkRequested_ = false;
			fireRequested_ = false;
			reloadRequested_ = false;
			inventorySlotRequested_ = -1;
			weaponSwitchRequested_ = 0;
			toggleFireModeRequested_ = false;
			meleeRequested_ = false;
		}

	private:
		::InputSnapshot frameInput_{};
		float moveX_ = 0.0f;
		float moveZ_ = 0.0f;
		float lookYawDelta_ = 0.0f;
		float lookPitchDelta_ = 0.0f;
		float adsLookSensitivityMultiplier_ = 0.55f;
		bool jumpRequested_ = false;
		bool blinkRequested_ = false;
		bool fireRequested_ = false;
		bool reloadRequested_ = false;
		bool meleeRequested_ = false;
		int inventorySlotRequested_ = -1;
		int weaponSwitchRequested_ = 0;
		bool toggleFireModeRequested_ = false;
		bool inputEnabled_ = true;
		bool restrictionsEnabled_ = false;
		bool allowMove_ = true;
		bool allowShoot_ = true;
		bool allowReload_ = true;
		bool weaponSwitchEnabled_ = true;
	};
} // namespace Ken4lowEngine
