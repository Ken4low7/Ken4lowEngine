#pragma once

#include "WeaponParams.h"

#include <ActorComponent.h>
#include <Vector3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace Ken4lowEngine
{
	/// Playerの6カテゴリ武器状態・射撃・Reload・ViewModel演出・Projectile設定を所有するComponent。
	class WeaponComponent final : public ActorComponent
	{
	public:
		static constexpr int kWeaponCount = 6;

		void Initialize() override
		{
			ActorComponent::Initialize();
			ResetWeapon();
		}

		void Update(float deltaTime) override
		{
			const float dt = (std::max)(0.0f, deltaTime);
			fireCooldownRemaining_ = (std::max)(0.0f, fireCooldownRemaining_ - dt);
			dryFireFeedbackRemaining_ = (std::max)(0.0f, dryFireFeedbackRemaining_ - dt);
			recoilTimer_ = (std::max)(0.0f, recoilTimer_ - dt);
			equipTimer_ = (std::min)(equipDuration_, equipTimer_ + dt);
			reticleKick_ = (std::max)(0.0f, reticleKick_ - reticleRecoverSpeed_ * dt);

			if (!weaponEnabled_)
			{
				ClearTransientRequests();
				triggerHeld_ = false;
				return;
			}

			if (toggleFireModeRequested_)
			{
				toggleFireModeRequested_ = false;
				if (!IsMeleeWeapon()) automaticFireMode_ = !automaticFireMode_;
			}

			if (isReloading_)
			{
				reloadTimer_ += dt;
				if (reloadTimer_ >= reloadDuration_) FinishReload();
			}
			if (reloadRequested_)
			{
				reloadRequested_ = false;
				StartReload();
			}
			if (fireRequested_)
			{
				fireRequested_ = false;
				TryFire();
			}
			if (automaticFireMode_ && triggerHeld_) TryFire();
		}

		void DrawImGui() override
		{
#ifdef USE_IMGUI
			ImGui::SeparatorText("武器");
			ImGui::Text("Weapon: %s  SlotID:%d", GetWeaponDisplayName(), weaponId_);
			ImGui::Text("Ammo: %d / %d  Reserve: %d / %d", magazineAmmo_, magazineCapacity_, reserveAmmo_, maxReserveAmmo_);
			ImGui::Text("State: %s / Equip: %.2f / Mode:%s", isReloading_ ? "Reloading" : "Ready", GetEquipNormalizedTime(), automaticFireMode_ ? "AUTO" : "SEMI");
			ImGui::Text("Damage: %.1f / Range: %.1f / Interval: %.3f", damage_, range_, fireInterval_);
			ImGui::Text("Shot Revision: %u / Equip Revision: %u", shotRevision_, equipRevision_);
#endif
		}

		std::string GetClassTypeName() const override { return "WeaponComponent"; }

		void ToJson(nlohmann::json& outJson) const override
		{
			ActorComponent::ToJson(outJson);
			outJson["WeaponId"] = weaponId_;
			outJson["MagazineAmmo"] = magazineAmmo_;
			outJson["ReserveAmmo"] = reserveAmmo_;
			outJson["AutomaticFireMode"] = automaticFireMode_;
			outJson["WeaponEnabled"] = weaponEnabled_;
		}

		void FromJson(const nlohmann::json& inJson) override
		{
			ActorComponent::FromJson(inJson);
			ResetAllStoredStates();
			weaponId_ = NormalizeWeaponId(inJson.value("WeaponId", weaponId_));
			LoadDefinition(weaponId_, false);
			if (!IsMeleeWeapon())
			{
				magazineAmmo_ = std::clamp(inJson.value("MagazineAmmo", magazineAmmo_), 0, magazineCapacity_);
				reserveAmmo_ = std::clamp(inJson.value("ReserveAmmo", reserveAmmo_), 0, maxReserveAmmo_);
			}
			automaticFireMode_ = inJson.value("AutomaticFireMode", automaticFireMode_);
			weaponEnabled_ = inJson.value("WeaponEnabled", weaponEnabled_);
			SaveCurrentWeaponState();
			ResetTransientState();
			equipTimer_ = equipDuration_;
		}

		void RequestFire() { fireRequested_ = true; }
		void RequestReload() { reloadRequested_ = true; }
		void RequestToggleFireMode() { toggleFireModeRequested_ = true; }
		void SetTriggerHeld(bool held) { triggerHeld_ = held && !IsMeleeWeapon(); }

		void SetWeaponId(int weaponId)
		{
			const int normalized = NormalizeWeaponId(weaponId);
			if (normalized == weaponId_) return;
			SaveCurrentWeaponState();
			weaponId_ = normalized;
			LoadDefinition(weaponId_, true);
			RestartEquipAnimation();
			++equipRevision_; // 選択スロット変更時にModel・HUD・入力種別を同じRevisionで切り替える。
		}

		void SetWeaponEnabled(bool enabled)
		{
			weaponEnabled_ = enabled;
			if (!enabled)
			{
				ClearTransientRequests();
				triggerHeld_ = false;
			}
		}

		void ConfigureAmmoState(int magazineCapacity, int magazineAmmo, int reserveAmmo, int maxReserveAmmo = 120)
		{
			if (IsMeleeWeapon()) return;
			magazineCapacity_ = (std::max)(1, magazineCapacity);
			magazineAmmo_ = std::clamp(magazineAmmo, 0, magazineCapacity_);
			maxReserveAmmo_ = (std::max)(reserveAmmo, maxReserveAmmo);
			reserveAmmo_ = std::clamp(reserveAmmo, 0, maxReserveAmmo_);
			SaveCurrentWeaponState();
			ResetTransientState();
		}

		int AddReserveAmmo(int amount)
		{
			if (IsMeleeWeapon()) return 0;
			const int before = reserveAmmo_;
			reserveAmmo_ = std::clamp(reserveAmmo_ + amount, 0, maxReserveAmmo_);
			SaveCurrentWeaponState();
			return reserveAmmo_ - before;
		}

		void ResetWeapon()
		{
			ResetAllStoredStates();
			weaponId_ = 0;
			LoadDefinition(weaponId_, false);
			weaponEnabled_ = true;
			ResetTransientState();
			equipTimer_ = equipDuration_;
			++equipRevision_; // 状態Resetは即時Readyにし、GamePlay開始演出はStartWeaponEquipAnimationから明示開始する。
		}

		void RestartEquipAnimation()
		{
			equipTimer_ = 0.0f;
			isReloading_ = false;
			reloadTimer_ = 0.0f;
			fireCooldownRemaining_ = 0.0f;
		}

		WeaponParams BuildProjectileParams() const
		{
			const WeaponDefinition& definition = GetDefinition(weaponId_);
			WeaponParams params{};
			params.weaponID = definition.masterWeaponId;
			params.weaponCategory = definition.category;
			params.deathKnockbackType = definition.knockbackType;
			params.deathKnockbackPower = definition.knockbackPower;
			params.deathKnockbackUpPower = definition.knockbackUpPower;
			params.deathExplosionRadius = definition.deathExplosionRadius;
			params.deathImpulseScale = 1.0f;
			params.isAutomatic = definition.automatic;
			params.canToggleFireMode = false;
			params.drawProjectileModel = definition.drawProjectileModel;
			params.damage = definition.damage;
			params.secPerShot = definition.fireInterval;
			params.magCapacity = definition.magazineCapacity;
			params.maxReserveAmmo = definition.maxReserveAmmo;
			params.reloadSec = definition.reloadDuration;
			params.tacticalReloadSec = definition.reloadDuration;
			params.emptyReloadSec = definition.reloadDuration;
			params.projectileSpeed = definition.projectileSpeed;
			params.projectileLifeTime = definition.projectileLifeTime;
			params.maxRange = definition.range;
			params.muzzleForwardOffset = definition.muzzleForwardOffset;
			params.splashRadius = definition.splashRadius;
			params.splashDamage = static_cast<int>(std::lround(definition.damage));
			params.baseHipSpreadDeg = definition.baseSpread;
			params.maxSpreadDeg = definition.maxSpread;
			params.spreadRecoveryRate = definition.spreadRecovery;
			return params;
		}

		int GetWeaponId() const { return weaponId_; }
		int GetMagazineAmmo() const { return magazineAmmo_; }
		int GetMagazineCapacity() const { return magazineCapacity_; }
		int GetReserveAmmo() const { return reserveAmmo_; }
		int GetMaxReserveAmmo() const { return maxReserveAmmo_; }
		float GetDamage() const { return damage_; }
		float GetRange() const { return range_; }
		float GetFireInterval() const { return fireInterval_; }
		float GetReloadTimer() const { return reloadTimer_; }
		float GetReloadDuration() const { return reloadDuration_; }
		bool IsReloading() const { return isReloading_; }
		bool IsWeaponEnabled() const { return weaponEnabled_; }
		bool IsAutomaticFireMode() const { return automaticFireMode_; }
		bool IsMeleeWeapon() const { return GetDefinition(weaponId_).meleeWeapon; }
		bool UsesAmmo() const { return !IsMeleeWeapon(); }
		bool IsEquipAnimating() const { return equipTimer_ < equipDuration_; }
		bool ShouldShowNoAmmoFeedback() const { return UsesAmmo() && (dryFireFeedbackRemaining_ > 0.0f || (magazineAmmo_ <= 0 && reserveAmmo_ <= 0)); }
		unsigned int GetShotRevision() const { return shotRevision_; }
		unsigned int GetEquipRevision() const { return equipRevision_; }
		const char* GetWeaponDisplayName() const { return GetDefinition(weaponId_).displayName; }
		const char* GetViewModelPath() const { return GetDefinition(weaponId_).modelPath; }
		float GetReticleSpread() const { return reticleBaseSpread_ + reticleKick_; }
		float GetReticleMaxSpread() const { return reticleMaxSpread_; }
		float GetEquipNormalizedTime() const { return equipDuration_ > 0.000001f ? std::clamp(equipTimer_ / equipDuration_, 0.0f, 1.0f) : 1.0f; }

		Vector3 GetViewModelPositionOffset() const
		{
			const float equipT = SmoothStep(GetEquipNormalizedTime());
			Vector3 offset{ 0.0f, -(1.0f - equipT) * 0.42f, -(1.0f - equipT) * 0.12f };
			if (isReloading_ && reloadDuration_ > 0.000001f)
			{
				const float arc = std::sin(std::clamp(reloadTimer_ / reloadDuration_, 0.0f, 1.0f) * kPi);
				offset.x += arc * 0.08f;
				offset.y -= arc * 0.18f;
			}
			if (recoilTimer_ > 0.0f && recoilDuration_ > 0.000001f) offset.z -= 0.10f * (recoilTimer_ / recoilDuration_);
			return offset;
		}

		Vector3 GetViewModelRotationOffset() const
		{
			const float equipT = SmoothStep(GetEquipNormalizedTime());
			Vector3 rotation{ (1.0f - equipT) * 0.45f, 0.0f, 0.0f };
			if (isReloading_ && reloadDuration_ > 0.000001f)
			{
				const float arc = std::sin(std::clamp(reloadTimer_ / reloadDuration_, 0.0f, 1.0f) * kPi);
				rotation.x += arc * 0.34f;
				rotation.z += arc * 0.20f;
			}
			if (recoilTimer_ > 0.0f && recoilDuration_ > 0.000001f) rotation.x -= 0.14f * (recoilTimer_ / recoilDuration_);
			return rotation;
		}

	private:
		struct WeaponDefinition
		{
			int masterWeaponId;
			const char* displayName;
			const char* modelPath;
			EWeaponCategory category;
			EDeathKnockbackType knockbackType;
			int magazineCapacity;
			int reserveAmmo;
			int maxReserveAmmo;
			float reloadDuration;
			float damage;
			float range;
			float fireInterval;
			float projectileSpeed;
			float projectileLifeTime;
			float muzzleForwardOffset;
			float splashRadius;
			float knockbackPower;
			float knockbackUpPower;
			float deathExplosionRadius;
			bool automatic;
			bool meleeWeapon;
			bool drawProjectileModel;
			float baseSpread;
			float maxSpread;
			float spreadPerShot;
			float spreadRecovery;
			float equipDuration;
		};

		struct StoredWeaponState
		{
			bool valid = false;
			int magazineAmmo = 0;
			int reserveAmmo = 0;
			bool automatic = false;
		};

		static const WeaponDefinition& GetDefinition(int weaponId)
		{
			static const std::array<WeaponDefinition, kWeaponCount> definitions = {
				WeaponDefinition{ 1, "M4A1", "Sources/Weapons/primary_rifle.gltf", EWeaponCategory::Primary, EDeathKnockbackType::Default, 30, 90, 120, 1.95f, 31.0f, 80.0f, 60.0f / 780.0f, 400.0f, 1.0f, 0.35f, 0.0f, 8.0f, 2.0f, 0.0f, true, false, false, 1.2f, 5.0f, 2.0f, 18.0f, 0.35f },
				WeaponDefinition{ 2, "Backup Pistol", "Sources/Weapons/backup_pistol.gltf", EWeaponCategory::Backup, EDeathKnockbackType::Light, 12, 48, 72, 1.35f, 42.0f, 65.0f, 0.25f, 320.0f, 1.0f, 0.30f, 0.0f, 6.0f, 1.5f, 0.0f, false, false, false, 0.8f, 4.0f, 1.4f, 20.0f, 0.24f },
				WeaponDefinition{ 3, "Knife", "Sources/Weapons/melee_knife.gltf", EWeaponCategory::Melee, EDeathKnockbackType::Light, 0, 0, 0, 0.0f, 45.0f, 2.0f, 0.10f, 0.0f, 0.0f, 0.0f, 0.0f, 12.0f, 1.5f, 0.0f, false, true, false, 0.0f, 0.0f, 0.0f, 18.0f, 0.35f },
				WeaponDefinition{ 4, "PlasmaCaster", "Sources/Weapons/special_launcher.gltf", EWeaponCategory::Special, EDeathKnockbackType::Heavy, 6, 18, 24, 2.10f, 55.0f, 45.0f, 60.0f / 90.0f, 190.0f, 1.20f, 0.30f, 1.60f, 12.0f, 4.0f, 0.0f, false, false, true, 0.4f, 1.2f, 2.0f, 10.0f, 0.35f },
				WeaponDefinition{ 5, "SR-01", "Sources/Weapons/sniper_rifle.gltf", EWeaponCategory::Sniper, EDeathKnockbackType::Sniper, 5, 15, 20, 2.60f, 300.0f, 180.0f, 60.0f / 42.0f, 1040.0f, 1.50f, 0.35f, 0.0f, 20.0f, 2.0f, 0.0f, false, false, false, 3.0f, 5.0f, 2.5f, 8.0f, 0.45f },
				WeaponDefinition{ 6, "RL-58", "Sources/Weapons/heavy_weapon.gltf", EWeaponCategory::Heavy, EDeathKnockbackType::Explosion, 1, 6, 12, 2.90f, 450.0f, 120.0f, 60.0f / 45.0f, 85.0f, 6.0f, 0.65f, 5.50f, 18.0f, 10.0f, 6.0f, false, false, true, 3.5f, 8.0f, 8.0f, 10.0f, 0.50f }
			};
			return definitions[static_cast<std::size_t>(NormalizeWeaponId(weaponId))];
		}

		static int NormalizeWeaponId(int weaponId) { return std::clamp(weaponId, 0, kWeaponCount - 1); }
		static float SmoothStep(float value) { return value * value * (3.0f - 2.0f * value); }
		void ResetAllStoredStates() { for (StoredWeaponState& state : storedStates_) state = {}; }

		void SaveCurrentWeaponState()
		{
			StoredWeaponState& state = storedStates_[static_cast<std::size_t>(NormalizeWeaponId(weaponId_))];
			state.valid = true;
			state.magazineAmmo = magazineAmmo_;
			state.reserveAmmo = reserveAmmo_;
			state.automatic = automaticFireMode_;
		}

		void LoadDefinition(int weaponId, bool restoreStoredState)
		{
			const WeaponDefinition& definition = GetDefinition(weaponId);
			magazineCapacity_ = definition.magazineCapacity;
			maxReserveAmmo_ = definition.maxReserveAmmo;
			reloadDuration_ = definition.reloadDuration;
			damage_ = definition.damage;
			range_ = definition.range;
			fireInterval_ = definition.fireInterval;
			reticleBaseSpread_ = definition.baseSpread;
			reticleMaxSpread_ = definition.maxSpread;
			reticleSpreadPerShot_ = definition.spreadPerShot;
			reticleRecoverSpeed_ = definition.spreadRecovery;
			equipDuration_ = definition.equipDuration;
			const StoredWeaponState& stored = storedStates_[static_cast<std::size_t>(NormalizeWeaponId(weaponId))];
			if (restoreStoredState && stored.valid)
			{
				magazineAmmo_ = std::clamp(stored.magazineAmmo, 0, magazineCapacity_);
				reserveAmmo_ = std::clamp(stored.reserveAmmo, 0, maxReserveAmmo_);
				automaticFireMode_ = stored.automatic;
			}
			else
			{
				magazineAmmo_ = magazineCapacity_;
				reserveAmmo_ = definition.reserveAmmo;
				automaticFireMode_ = definition.automatic;
				SaveCurrentWeaponState();
			}
			ResetTransientState();
		}

		bool TryFire()
		{
			if (IsMeleeWeapon() || isReloading_ || IsEquipAnimating() || fireCooldownRemaining_ > 0.0f) return false;
			if (magazineAmmo_ <= 0)
			{
				dryFireFeedbackRemaining_ = 0.35f;
				return false;
			}
			--magazineAmmo_;
			fireCooldownRemaining_ = fireInterval_;
			recoilTimer_ = recoilDuration_;
			reticleKick_ = (std::min)((std::max)(0.0f, reticleMaxSpread_ - reticleBaseSpread_), reticleKick_ + reticleSpreadPerShot_);
			++shotRevision_;
			SaveCurrentWeaponState();
			return true;
		}

		bool StartReload()
		{
			if (IsMeleeWeapon() || isReloading_ || IsEquipAnimating() || magazineAmmo_ >= magazineCapacity_ || reserveAmmo_ <= 0) return false;
			isReloading_ = true;
			reloadTimer_ = 0.0f;
			dryFireFeedbackRemaining_ = 0.0f;
			return true;
		}

		void FinishReload()
		{
			const int loaded = (std::min)(magazineCapacity_ - magazineAmmo_, reserveAmmo_);
			magazineAmmo_ += loaded;
			reserveAmmo_ -= loaded;
			isReloading_ = false;
			reloadTimer_ = 0.0f;
			SaveCurrentWeaponState();
		}

		void ClearTransientRequests()
		{
			fireRequested_ = false;
			reloadRequested_ = false;
			toggleFireModeRequested_ = false;
		}

		void ResetTransientState()
		{
			isReloading_ = false;
			reloadTimer_ = 0.0f;
			fireCooldownRemaining_ = 0.0f;
			recoilTimer_ = 0.0f;
			reticleKick_ = 0.0f;
			dryFireFeedbackRemaining_ = 0.0f;
			triggerHeld_ = false;
			ClearTransientRequests();
		}

	private:
		static constexpr float kPi = 3.14159265358979323846f;
		std::array<StoredWeaponState, kWeaponCount> storedStates_{};
		int weaponId_ = 0;
		int magazineCapacity_ = 30;
		int magazineAmmo_ = 30;
		int reserveAmmo_ = 90;
		int maxReserveAmmo_ = 120;
		float reloadDuration_ = 1.95f;
		float reloadTimer_ = 0.0f;
		float damage_ = 31.0f;
		float range_ = 80.0f;
		float fireInterval_ = 60.0f / 780.0f;
		float fireCooldownRemaining_ = 0.0f;
		float equipDuration_ = 0.35f;
		float equipTimer_ = 0.35f;
		float recoilDuration_ = 0.10f;
		float recoilTimer_ = 0.0f;
		float reticleBaseSpread_ = 1.2f;
		float reticleMaxSpread_ = 5.0f;
		float reticleSpreadPerShot_ = 2.0f;
		float reticleRecoverSpeed_ = 18.0f;
		float reticleKick_ = 0.0f;
		float dryFireFeedbackRemaining_ = 0.0f;
		bool weaponEnabled_ = true;
		bool isReloading_ = false;
		bool automaticFireMode_ = true;
		bool triggerHeld_ = false;
		bool fireRequested_ = false;
		bool reloadRequested_ = false;
		bool toggleFireModeRequested_ = false;
		unsigned int shotRevision_ = 0u;
		unsigned int equipRevision_ = 0u;
	};
} // namespace Ken4lowEngine
