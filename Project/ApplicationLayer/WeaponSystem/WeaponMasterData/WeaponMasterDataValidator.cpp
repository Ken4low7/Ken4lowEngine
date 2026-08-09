#include "WeaponMasterDataValidator.h"

#include <algorithm>

namespace
{
	const char* FireModeToString(EFireMode mode)
	{
		switch (mode)
		{
		case EFireMode::SemiAuto: return "SemiAuto";
		case EFireMode::Burst: return "Burst";
		case EFireMode::FullAuto: return "FullAuto";
		case EFireMode::Charge: return "Charge";
		default: return "Unknown";
		}
	}

	bool ContainsFireMode(const std::vector<EFireMode>& modes, EFireMode target)
	{
		return std::find(modes.begin(), modes.end(), target) != modes.end();
	}
}

/// -------------------------------------------------------------
///				　		武器マスターデータ検証
/// -------------------------------------------------------------
bool WeaponMasterDataValidator::Validate(const FWeaponMasterData& data, std::string* outError)
{
	if (outError) outError->clear();

	bool ok = true;

	/// ---------- コアデータ検証 ---------- ///
	if (data.coreData.weaponID <= 0) { ok = false; Append(outError, "coreData.weaponID must be > 0"); }
	if (data.coreData.weaponName.empty()) { ok = false; Append(outError, "coreData.weaponName is empty"); }

	/// ---------- 経済データ検証 ---------- ///
	if (data.economyData.purchasePrice < 0) { ok = false; Append(outError, "economyData.purchasePrice must be >= 0"); }
	if (data.economyData.minLevelToUnlock < 0) { ok = false; Append(outError, "economyData.minLevelToUnlock must be >= 0"); }
	if (data.economyData.discountRate < 0.0f || data.economyData.discountRate > 1.0f)
	{
		ok = false; Append(outError, "economyData.discountRate must be in [0, 1]");
	}
	for (size_t i = 0; i < data.economyData.upgradeCosts.size(); ++i)
	{
		if (data.economyData.upgradeCosts[i] < 0)
		{
			ok = false;
			Append(outError, "economyData.upgradeCosts contains negative value");
			break;
		}
	}

	/// ---------- 射撃性能データ検証 ---------- ///
	if (data.stats.damage < 0.0f) { ok = false; Append(outError, "stats.damage must be >= 0"); }
	if (data.stats.minDamage < 0.0f) { ok = false; Append(outError, "stats.minDamage must be >= 0"); }
	if (data.stats.minDamage > data.stats.damage) { ok = false; Append(outError, "stats.minDamage must be <= stats.damage"); }

	if (data.stats.damageFalloffStart < 0.0f) { ok = false; Append(outError, "stats.damageFalloffStart must be >= 0"); }
	if (data.stats.damageFalloffEnd < 0.0f) { ok = false; Append(outError, "stats.damageFalloffEnd must be >= 0"); }
	if (data.stats.damageFalloffEnd > 0.0f && data.stats.damageFalloffEnd < data.stats.damageFalloffStart)
	{
		ok = false; Append(outError, "stats.damageFalloffEnd must be >= stats.damageFalloffStart");
	}

	if (data.stats.fireRate < 0.0f) { ok = false; Append(outError, "stats.fireRate must be >= 0"); }
	if (data.stats.mobility < 0.0f) { ok = false; Append(outError, "stats.mobility must be >= 0"); }
	if (data.stats.capacity < 0) { ok = false; Append(outError, "stats.capacity must be >= 0"); }

	if (data.stats.reloadTime < 0.0f) { ok = false; Append(outError, "stats.reloadTime must be >= 0"); }
	if (data.stats.tacticalReloadTime < 0.0f) { ok = false; Append(outError, "stats.tacticalReloadTime must be >= 0"); }
	if (data.stats.emptyReloadTime < 0.0f) { ok = false; Append(outError, "stats.emptyReloadTime must be >= 0"); }

	if (data.stats.ammoPerShot <= 0) { ok = false; Append(outError, "stats.ammoPerShot must be > 0"); }
	if (data.stats.maxReserveAmmo < 0) { ok = false; Append(outError, "stats.maxReserveAmmo must be >= 0"); }
	if (data.stats.chamberSize < 0) { ok = false; Append(outError, "stats.chamberSize must be >= 0"); }
	if (data.stats.bHasChamber && data.stats.chamberSize <= 0)
	{
		ok = false; Append(outError, "stats.chamberSize must be > 0 when stats.bHasChamber is true");
	}

	if (data.stats.criticalChance < 0.0f || data.stats.criticalChance > 1.0f)
	{
		ok = false; Append(outError, "stats.criticalChance must be in [0, 1]");
	}
	if (data.stats.headshotMultiplier < 0.0f) { ok = false; Append(outError, "stats.headshotMultiplier must be >= 0"); }
	if (data.stats.bodyMultiplier < 0.0f) { ok = false; Append(outError, "stats.bodyMultiplier must be >= 0"); }
	if (data.stats.armMultiplier < 0.0f) { ok = false; Append(outError, "stats.armMultiplier must be >= 0"); }
	if (data.stats.legMultiplier < 0.0f) { ok = false; Append(outError, "stats.legMultiplier must be >= 0"); }

	if (data.stats.pelletCount <= 0) { ok = false; Append(outError, "stats.pelletCount must be > 0"); }
	if (data.stats.pelletSpreadAngle < 0.0f) { ok = false; Append(outError, "stats.pelletSpreadAngle must be >= 0"); }

	/// ---------- 操作・反動データ検証 ---------- ///
	if (data.handling.accuracy < 0.0f || data.handling.accuracy > 1.0f)
	{
		ok = false; Append(outError, "handling.accuracy must be in [0, 1]");
	}

	if (data.handling.spreadIncrease < 0.0f) { ok = false; Append(outError, "handling.spreadIncrease must be >= 0"); }
	if (data.handling.baseHipSpread < 0.0f) { ok = false; Append(outError, "handling.baseHipSpread must be >= 0"); }
	if (data.handling.baseAdsSpread < 0.0f) { ok = false; Append(outError, "handling.baseAdsSpread must be >= 0"); }
	if (data.handling.moveSpreadMultiplier < 0.0f) { ok = false; Append(outError, "handling.moveSpreadMultiplier must be >= 0"); }
	if (data.handling.jumpSpreadMultiplier < 0.0f) { ok = false; Append(outError, "handling.jumpSpreadMultiplier must be >= 0"); }
	if (data.handling.crouchSpreadMultiplier < 0.0f) { ok = false; Append(outError, "handling.crouchSpreadMultiplier must be >= 0"); }
	if (data.handling.spreadRecoveryRate < 0.0f) { ok = false; Append(outError, "handling.spreadRecoveryRate must be >= 0"); }
	if (data.handling.maxSpread < 0.0f) { ok = false; Append(outError, "handling.maxSpread must be >= 0"); }

	if (data.handling.verticalRecoil < 0.0f) { ok = false; Append(outError, "handling.verticalRecoil must be >= 0"); }
	if (data.handling.horizontalRecoil < 0.0f) { ok = false; Append(outError, "handling.horizontalRecoil must be >= 0"); }
	if (data.handling.cameraRecoilPitch < 0.0f) { ok = false; Append(outError, "handling.cameraRecoilPitch must be >= 0"); }
	if (data.handling.cameraRecoilYaw < 0.0f) { ok = false; Append(outError, "handling.cameraRecoilYaw must be >= 0"); }
	if (data.handling.weaponKickBack < 0.0f) { ok = false; Append(outError, "handling.weaponKickBack must be >= 0"); }
	if (data.handling.recoilRecovery < 0.0f) { ok = false; Append(outError, "handling.recoilRecovery must be >= 0"); }
	if (data.handling.recoilResetDelay < 0.0f) { ok = false; Append(outError, "handling.recoilResetDelay must be >= 0"); }

	if (data.handling.adsZoomFov < 0.0f) { ok = false; Append(outError, "handling.adsZoomFov must be >= 0"); }
	if (data.handling.zoomLevel < 0.0f) { ok = false; Append(outError, "handling.zoomLevel must be >= 0"); }
	if (data.handling.adsTransitionSpeed < 0.0f) { ok = false; Append(outError, "handling.adsTransitionSpeed must be >= 0"); }
	if (data.handling.adsInTime < 0.0f) { ok = false; Append(outError, "handling.adsInTime must be >= 0"); }
	if (data.handling.adsOutTime < 0.0f) { ok = false; Append(outError, "handling.adsOutTime must be >= 0"); }
	if (data.handling.adsMoveSpeedMultiplier < 0.0f) { ok = false; Append(outError, "handling.adsMoveSpeedMultiplier must be >= 0"); }

	if (data.handling.equipTime < 0.0f) { ok = false; Append(outError, "handling.equipTime must be >= 0"); }
	if (data.handling.unequipTime < 0.0f) { ok = false; Append(outError, "handling.unequipTime must be >= 0"); }
	if (data.handling.sprintToFireTime < 0.0f) { ok = false; Append(outError, "handling.sprintToFireTime must be >= 0"); }
	if (data.handling.fireToSprintTime < 0.0f) { ok = false; Append(outError, "handling.fireToSprintTime must be >= 0"); }
	if (data.handling.fixedDelayTime < 0.0f) { ok = false; Append(outError, "handling.fixedDelayTime must be >= 0"); }

	/// ---------- レティクルデータ検証 ---------- ///
	if (data.reticleData.reticleBaseSize < 0.0f) { ok = false; Append(outError, "reticleData.reticleBaseSize must be >= 0"); }
	if (data.reticleData.reticleMaxSize < 0.0f) { ok = false; Append(outError, "reticleData.reticleMaxSize must be >= 0"); }
	if (data.reticleData.reticleExpandPerShot < 0.0f) { ok = false; Append(outError, "reticleData.reticleExpandPerShot must be >= 0"); }
	if (data.reticleData.reticleRecoverSpeed < 0.0f) { ok = false; Append(outError, "reticleData.reticleRecoverSpeed must be >= 0"); }
	if (data.reticleData.reticleMaxSize < data.reticleData.reticleBaseSize)
	{
		ok = false; Append(outError, "reticleData.reticleMaxSize must be >= reticleData.reticleBaseSize");
	}

	/// ---------- 近接/射撃武器の整合性 ---------- ///
	const bool categoryIsMelee = (data.coreData.category == EWeaponCategory::Melee);

	if (categoryIsMelee)
	{
		// 近接なら meleeData 必須
		if (!data.meleeData.has_value())
		{
			ok = false; Append(outError, "Melee weapon requires meleeData");
		}

		// 近接は弾薬なし想定
		if (data.stats.ammoType != EAmmoType::None)
		{
			ok = false; Append(outError, "Melee weapon stats.ammoType should be None");
		}

		// 近接は projectileData 不要
		if (data.projectileData.has_value())
		{
			ok = false; Append(outError, "Melee weapon projectileData should be null");
		}
	}
	else
	{
		// 射撃武器に meleeData が入っていたら設計ミスの可能性が高い
		if (data.meleeData.has_value())
		{
			ok = false; Append(outError, "Non-melee weapon should not have meleeData");
		}

		// 射撃武器は弾薬あり
		if (data.stats.ammoType == EAmmoType::None)
		{
			ok = false; Append(outError, "Ranged weapon: stats.ammoType must not be None");
		}
		if (data.stats.capacity <= 0)
		{
			ok = false; Append(outError, "Ranged weapon: stats.capacity must be > 0");
		}
		if (data.stats.ammoPerShot <= 0)
		{
			ok = false; Append(outError, "Ranged weapon: stats.ammoPerShot must be > 0");
		}
	}

	/// ---------- 近接武器データ検証（存在時） ---------- ///
	if (data.meleeData)
	{
		const auto& m = *data.meleeData;

		if (m.attackRange < 0.0f) { ok = false; Append(outError, "meleeData.attackRange must be >= 0"); }
		if (m.attackArc < 0.0f || m.attackArc > 360.0f) { ok = false; Append(outError, "meleeData.attackArc must be in [0, 360]"); }
		if (m.verticalAngle < 0.0f || m.verticalAngle > 180.0f) { ok = false; Append(outError, "meleeData.verticalAngle must be in [0, 180]"); }

		if (m.startupDelay < 0.0f) { ok = false; Append(outError, "meleeData.startupDelay must be >= 0"); }
		if (m.activeFrames < 0.0f) { ok = false; Append(outError, "meleeData.activeFrames must be >= 0"); }
		if (m.recoveryDelay < 0.0f) { ok = false; Append(outError, "meleeData.recoveryDelay must be >= 0"); }

		if (m.maxComboCount <= 0) { ok = false; Append(outError, "meleeData.maxComboCount must be > 0"); }
		if (m.comboWindow < 0.0f) { ok = false; Append(outError, "meleeData.comboWindow must be >= 0"); }

		if (m.dashImpulse < 0.0f) { ok = false; Append(outError, "meleeData.dashImpulse must be >= 0"); }
		if (m.maxChargeTime < 0.0f) { ok = false; Append(outError, "meleeData.maxChargeTime must be >= 0"); }
		if (m.chargeDamageMultiplier < 0.0f) { ok = false; Append(outError, "meleeData.chargeDamageMultiplier must be >= 0"); }
		if (m.bCanCharge && m.maxChargeTime <= 0.0f)
		{
			ok = false; Append(outError, "meleeData.maxChargeTime must be > 0 when meleeData.bCanCharge is true");
		}

		if (m.blockDamageReduction < 0.0f || m.blockDamageReduction > 1.0f)
		{
			ok = false; Append(outError, "meleeData.blockDamageReduction must be in [0, 1]");
		}

		if (m.hitStopDuration < 0.0f) { ok = false; Append(outError, "meleeData.hitStopDuration must be >= 0"); }
		if (m.knockbackForce < 0.0f) { ok = false; Append(outError, "meleeData.knockbackForce must be >= 0"); }
	}

	/// ---------- 弾道データ検証（存在時） ---------- ///
	if (data.projectileData)
	{
		const auto& p = *data.projectileData;

		if (p.maxRange < 0.0f) { ok = false; Append(outError, "projectileData.maxRange must be >= 0"); }
		if (p.gravityScale < 0.0f) { ok = false; Append(outError, "projectileData.gravityScale must be >= 0"); }
		if (p.projectileLifeTime < 0.0f) { ok = false; Append(outError, "projectileData.projectileLifeTime must be >= 0"); }
		if (p.projectileDrag < 0.0f) { ok = false; Append(outError, "projectileData.projectileDrag must be >= 0"); }
		if (p.projectileRadius < 0.0f) { ok = false; Append(outError, "projectileData.projectileRadius must be >= 0"); }
		if (p.traceRadius < 0.0f) { ok = false; Append(outError, "projectileData.traceRadius must be >= 0"); }
		if (p.spawnForwardOffset < 0.0f) { ok = false; Append(outError, "projectileData.spawnForwardOffset must be >= 0"); }

		if (p.bIsProjectile && p.projectileSpeed <= 0.0f)
		{
			ok = false; Append(outError, "projectileData.projectileSpeed must be > 0 for projectile weapons");
		}
		if (!p.bIsProjectile && p.projectileSpeed < 0.0f)
		{
			ok = false; Append(outError, "projectileData.projectileSpeed must be >= 0");
		}

		if (p.pierceCount < 0) { ok = false; Append(outError, "projectileData.pierceCount must be >= 0"); }
		if (p.ricochetCount < 0) { ok = false; Append(outError, "projectileData.ricochetCount must be >= 0"); }
		if (p.splashRadius < 0.0f) { ok = false; Append(outError, "projectileData.splashRadius must be >= 0"); }
	}

	/// ---------- 射撃モード整合性検証 ---------- ///
	if (data.supportedFireModels.empty())
	{
		ok = false; Append(outError, "supportedFireModels must not be empty");
	}
	else
	{
		if (!ContainsFireMode(data.supportedFireModels, data.defaultFireMode))
		{
			ok = false;
			Append(outError, std::string("defaultFireMode (") + FireModeToString(data.defaultFireMode) +
				") must be included in supportedFireModels");
		}
	}

	const bool supportsBurst = ContainsFireMode(data.supportedFireModels, EFireMode::Burst);
	const bool supportsCharge = ContainsFireMode(data.supportedFireModels, EFireMode::Charge);

	if (supportsBurst && !data.burstSettings.has_value())
	{
		ok = false; Append(outError, "supportedFireModels contains Burst but burstSettings is null");
	}
	if (supportsCharge && !data.chargeSettings.has_value())
	{
		ok = false; Append(outError, "supportedFireModels contains Charge but chargeSettings is null");
	}

	/// ---------- バースト / チャージ設定検証 ---------- ///
	if (data.burstSettings)
	{
		if (data.burstSettings->count <= 0) { ok = false; Append(outError, "burstSettings.count must be > 0"); }
		if (data.burstSettings->interval < 0.0f) { ok = false; Append(outError, "burstSettings.interval must be >= 0"); }
	}

	if (data.chargeSettings)
	{
		if (data.chargeSettings->maxChargeTime <= 0.0f)
		{
			ok = false; Append(outError, "chargeSettings.maxChargeTime must be > 0");
		}
	}

	/// ---------- 旧フラグとの軽い整合性（任意だが事故防止） ---------- ///
	if (data.bIsAutomatic && !ContainsFireMode(data.supportedFireModels, EFireMode::FullAuto))
	{
		ok = false; Append(outError, "bIsAutomatic is true, but supportedFireModels does not contain FullAuto");
	}

	return ok;
}

/// -------------------------------------------------------------
///		エラーメッセージを追加し、操作の成否を返す
/// -------------------------------------------------------------
void WeaponMasterDataValidator::Append(std::string* outError, const std::string& msg)
{
	if (!outError) return;
	if (!outError->empty()) *outError += "\n";
	*outError += msg;
}
