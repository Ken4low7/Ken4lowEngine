#include "WeaponMasterDataWriter.h"
#include <fstream>
#include <algorithm>
#include <json.hpp>
#include "WeaponMasterData.h"
#include "PathUtil.h"

using nlohmann::json;

namespace
{
	static const char* CategoryFolder(EWeaponCategory c)
	{
		switch (c)
		{
		case EWeaponCategory::Primary: return "primary";
		case EWeaponCategory::Backup:  return "backup";
		case EWeaponCategory::Melee:   return "melee";
		case EWeaponCategory::Special: return "special";
		case EWeaponCategory::Sniper:  return "sniper";
		case EWeaponCategory::Heavy:   return "heavy";
		default: return "unknown";
		}
	}

	static std::string RarityToStr(EWeaponRarity r)
	{
		switch (r)
		{
		case EWeaponRarity::Common:    return "common";
		case EWeaponRarity::Rare:      return "rare";
		case EWeaponRarity::Epic:      return "epic";
		case EWeaponRarity::Legendary: return "legendary";
		case EWeaponRarity::Mythical:  return "mythical";
		default: return "common";
		}
	}

	static std::string AmmoToStr(EAmmoType a)
	{
		switch (a)
		{
		case EAmmoType::Default:   return "default";
		case EAmmoType::Energy:    return "energy";
		case EAmmoType::Explosive: return "explosive";
		case EAmmoType::None:      return "none";
		default: return "default";
		}
	}

	static std::string CategoryToStr(EWeaponCategory c)
	{
		return CategoryFolder(c);
	}

	static std::string FireModeToStr(EFireMode m)
	{
		switch (m)
		{
		case EFireMode::SemiAuto: return "semi";
		case EFireMode::Burst:    return "burst";
		case EFireMode::FullAuto: return "fullauto";
		case EFireMode::Charge:   return "charge";
		default: return "semi";
		}
	}

	static std::string ReticleTypeToStr(EReticleType t)
	{
		switch (t)
		{
		case EReticleType::None:   return "none";
		case EReticleType::Dot:    return "dot";
		case EReticleType::Cross:  return "cross";
		case EReticleType::Circle: return "circle";
		case EReticleType::Scope:  return "scope";
		default: return "cross";
		}
	}

	static std::string AttributeToStr(EWeaponAttribute a)
	{
		switch (a)
		{
		case EWeaponAttribute::None:       return "none";
		case EWeaponAttribute::Poison:     return "poison";
		case EWeaponAttribute::Burning:    return "burning";
		case EWeaponAttribute::AreaDamage: return "area_damage";
		case EWeaponAttribute::Bouncing:   return "bouncing";
		case EWeaponAttribute::LifeSteal:  return "life_steal";
		case EWeaponAttribute::WallBreak:  return "wall_break";
		case EWeaponAttribute::FixedDelay: return "fixed_delay";
		default: return "none";
		}
	}
	static std::string DeathKnockbackTypeToStr(EDeathKnockbackType t)
	{
		switch (t)
		{
		case EDeathKnockbackType::Default: return "default";
		case EDeathKnockbackType::Light: return "light";
		case EDeathKnockbackType::Sniper: return "sniper";
		case EDeathKnockbackType::Heavy: return "heavy";
		case EDeathKnockbackType::Explosion: return "explosion";
		default: return "default";
		}
	}

	static bool EndsWith(const std::string& str, const std::string& suffix)
	{
		if (str.size() < suffix.size()) return false;
		return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
	}
}

/// -------------------------------------------------------------
///		武器マスターデータベース内の全データをカテゴリ別に rootDir 配下へ保存します
/// -------------------------------------------------------------
bool WeaponMasterDataWriter::SaveAllByCategory(const WeaponMasterDataDatabase& database, const std::filesystem::path& rootDir, std::string* outError)
{
	for (const auto& [id, d] : database.GetAll())
	{
		// 古いファイルを削除
		DeleteFilesForWeapon(rootDir, d, outError);

		const auto file = MakeWeaponFilePath(rootDir, d);
		if (!SaveOne(file, d, outError)) return false;
	}
	return true;
}

/// -------------------------------------------------------------
///		指定されたファイルパスに武器マスターデータを保存する
/// -------------------------------------------------------------
bool WeaponMasterDataWriter::SaveOne(const std::filesystem::path& filePath, const FWeaponMasterData& data, std::string* outError)
{
	json j;

	// ---------- coreData ----------
	j["coreData"] = {
		{"weaponID", data.coreData.weaponID},
		{"weaponName", data.coreData.weaponName},
		{"category", CategoryToStr(data.coreData.category)},
		{"rarity", RarityToStr(data.coreData.rarity)},
	};

	// ---------- economyData ----------
	j["economyData"] = {
		{"currencyType", data.economyData.currencyType},
		{"purchasePrice", data.economyData.purchasePrice},
		{"minLevelToUnlock", data.economyData.minLevelToUnlock},
		{"upgradeCosts", data.economyData.upgradeCosts},
		{"bIsLimitedTime", data.economyData.bIsLimitedTime},
		{"discountRate", data.economyData.discountRate},
	};

	// ---------- stats ----------
	j["stats"] = {
		{"damage", data.stats.damage},
		{"minDamage", data.stats.minDamage},
		{"damageFalloffStart", data.stats.damageFalloffStart},
		{"damageFalloffEnd", data.stats.damageFalloffEnd},

		{"fireRate", data.stats.fireRate},
		{"mobility", data.stats.mobility},
		{"capacity", data.stats.capacity},
		{"ammoType", AmmoToStr(data.stats.ammoType)},

		{"reloadTime", data.stats.reloadTime},
		{"tacticalReloadTime", data.stats.tacticalReloadTime},
		{"emptyReloadTime", data.stats.emptyReloadTime},
		{"bReloadByShell", data.stats.bReloadByShell},
		{"bCanInterruptReload", data.stats.bCanInterruptReload},

		{"ammoPerShot", data.stats.ammoPerShot},
		{"maxReserveAmmo", data.stats.maxReserveAmmo},
		{"bHasChamber", data.stats.bHasChamber},
		{"chamberSize", data.stats.chamberSize},

		{"criticalChance", data.stats.criticalChance},
		{"headshotMultiplier", data.stats.headshotMultiplier},
		{"bodyMultiplier", data.stats.bodyMultiplier},
		{"armMultiplier", data.stats.armMultiplier},
		{"legMultiplier", data.stats.legMultiplier},

		{"pelletCount", data.stats.pelletCount},
		{"pelletSpreadAngle", data.stats.pelletSpreadAngle},
	};

	// ---------- handling ----------
	j["handling"] = {
		{"accuracy", data.handling.accuracy},
		{"spreadIncrease", data.handling.spreadIncrease},

		{"baseHipSpread", data.handling.baseHipSpread},
		{"baseAdsSpread", data.handling.baseAdsSpread},
		{"moveSpreadMultiplier", data.handling.moveSpreadMultiplier},
		{"jumpSpreadMultiplier", data.handling.jumpSpreadMultiplier},
		{"crouchSpreadMultiplier", data.handling.crouchSpreadMultiplier},
		{"spreadRecoveryRate", data.handling.spreadRecoveryRate},
		{"maxSpread", data.handling.maxSpread},

		{"verticalRecoil", data.handling.verticalRecoil},
		{"horizontalRecoil", data.handling.horizontalRecoil},
		{"cameraRecoilPitch", data.handling.cameraRecoilPitch},
		{"cameraRecoilYaw", data.handling.cameraRecoilYaw},
		{"weaponKickBack", data.handling.weaponKickBack},
		{"recoilRecovery", data.handling.recoilRecovery},
		{"recoilResetDelay", data.handling.recoilResetDelay},

		{"adsZoomFov", data.handling.adsZoomFov},
		{"zoomLevel", data.handling.zoomLevel},
		{"adsTransitionSpeed", data.handling.adsTransitionSpeed},
		{"adsInTime", data.handling.adsInTime},
		{"adsOutTime", data.handling.adsOutTime},
		{"adsMoveSpeedMultiplier", data.handling.adsMoveSpeedMultiplier},

		{"equipTime", data.handling.equipTime},
		{"unequipTime", data.handling.unequipTime},
		{"sprintToFireTime", data.handling.sprintToFireTime},
		{"fireToSprintTime", data.handling.fireToSprintTime},
		{"fixedDelayTime", data.handling.fixedDelayTime},
	};

	// ---------- reticleData ----------
	j["reticleData"] = {
		{"reticleTexturePath", data.reticleData.reticleTexturePath},
		{"reticleType", ReticleTypeToStr(data.reticleData.reticleType)},
		{"reticleBaseSize", data.reticleData.reticleBaseSize},
		{"reticleMaxSize", data.reticleData.reticleMaxSize},
		{"reticleExpandPerShot", data.reticleData.reticleExpandPerShot},
		{"reticleRecoverSpeed", data.reticleData.reticleRecoverSpeed},

		{"bEnableMoveReticleExpand", data.reticleData.bEnableMoveReticleExpand},
		{"moveExpandMultiplier", data.reticleData.moveExpandMultiplier},
		{"sprintExpandMultiplier", data.reticleData.sprintExpandMultiplier},
		{"airExpandMultiplier", data.reticleData.airExpandMultiplier},
		{"landExpandImpulse", data.reticleData.landExpandImpulse},

		{"bHideReticleInADS", data.reticleData.bHideReticleInADS},
		{"bUseAdsReticleOverride", data.reticleData.bUseAdsReticleOverride},
		{"adsReticleTexturePath", data.reticleData.adsReticleTexturePath},
		{"bUseAdsCenterDot", data.reticleData.bUseAdsCenterDot},
		{"adsCenterDotTexturePath", data.reticleData.adsCenterDotTexturePath},
		{"adsReticleBlendTime", data.reticleData.adsReticleBlendTime},

		{"bShowHitMarker", data.reticleData.bShowHitMarker},
		{"hitMarkerTexturePath", data.reticleData.hitMarkerTexturePath},
		{"bUseHeadshotMarker", data.reticleData.bUseHeadshotMarker},
		{"headshotHitMarkerTexturePath", data.reticleData.headshotHitMarkerTexturePath},
		{"bUseKillConfirmMarker", data.reticleData.bUseKillConfirmMarker},
		{"killConfirmMarkerTexturePath", data.reticleData.killConfirmMarkerTexturePath},
		{"hitMarkerDuration", data.reticleData.hitMarkerDuration},
		{"killConfirmDuration", data.reticleData.killConfirmDuration},
	};

	// ---------- assetData ----------
	j["assetData"] = {
		{"modelPath", K4E::PathUtil::ToModelRelativePath(data.assetData.modelPath)},
		{"iconPath", data.assetData.iconPath},
	};

	// ---------- soundData ----------
	j["soundData"] = {
		{"fireSoundPath", data.soundData.fireSoundPath},
		{"reloadSoundPath", data.soundData.reloadSoundPath},
		{"emptySoundPath", data.soundData.emptySoundPath},
		{"equipSoundPath", data.soundData.equipSoundPath},
		{"impactSoundPath", data.soundData.impactSoundPath},
	};

	// ---------- vfxData ----------
	j["vfxData"] = {
		{"muzzleFlashVfxPath", data.vfxData.muzzleFlashVfxPath},
		{"tracerVfxPath", data.vfxData.tracerVfxPath},
		{"impactVfxPath", data.vfxData.impactVfxPath},
		{"shellEjectVfxPath", data.vfxData.shellEjectVfxPath},

		{"reloadVfxPath", data.vfxData.reloadVfxPath},
		{"chargeVfxPath", data.vfxData.chargeVfxPath},

		{"meleeSwingVfxPath", data.vfxData.meleeSwingVfxPath},
		{"meleeHitVfxPath", data.vfxData.meleeHitVfxPath},

		{"muzzleFlashScale", data.vfxData.muzzleFlashScale},
		{"tracerScale", data.vfxData.tracerScale},
		{"impactScale", data.vfxData.impactScale},
		{"meleeSwingScale", data.vfxData.meleeSwingScale},
		{"meleeHitScale", data.vfxData.meleeHitScale},
	};
	j["deathReaction"] = {
		{"type", DeathKnockbackTypeToStr(data.deathReaction.type)},
		{"power", data.deathReaction.power},
		{"upPower", data.deathReaction.upPower},
		{"explosionRadius", data.deathReaction.explosionRadius},
		{"impulseScale", data.deathReaction.impulseScale},
	};

	// ---------- socketData ----------
	j["socketData"] = {
		{"weaponAttachSocket", data.socketData.weaponAttachSocket},
		{"rightHandSocket", data.socketData.rightHandSocket},
		{"leftHandIkSocket", data.socketData.leftHandIkSocket},
		{"adsCameraSocket", data.socketData.adsCameraSocket},
		{"magazineSocket", data.socketData.magazineSocket},

		{"muzzleSocket", data.socketData.muzzleSocket},
		{"shellEjectSocket", data.socketData.shellEjectSocket},
		{"tracerStartSocket", data.socketData.tracerStartSocket},
		{"scopeSocket", data.socketData.scopeSocket},

		{"meleeTraceStartSocket", data.socketData.meleeTraceStartSocket},
		{"meleeTraceEndSocket", data.socketData.meleeTraceEndSocket},
		{"meleeHitSocket", data.socketData.meleeHitSocket},
	};

	// ---------- optional: projectileData ----------
	if (data.projectileData)
	{
		j["projectileData"] = {
			{"maxRange", data.projectileData->maxRange},
			{"bIsProjectile", data.projectileData->bIsProjectile},
			{"projectileSpeed", data.projectileData->projectileSpeed},
			{"gravityScale", data.projectileData->gravityScale},
			{"projectileLifeTime", data.projectileData->projectileLifeTime},
			{"projectileDrag", data.projectileData->projectileDrag},
			{"projectileRadius", data.projectileData->projectileRadius},
			{"traceRadius", data.projectileData->traceRadius},
			{"spawnForwardOffset", data.projectileData->spawnForwardOffset},
			{"pierceCount", data.projectileData->pierceCount},
			{"ricochetCount", data.projectileData->ricochetCount},
			{"splashRadius", data.projectileData->splashRadius},
			{"bCanDamageSelf", data.projectileData->bCanDamageSelf},
		};
	}

	// ---------- optional: burstSettings ----------
	if (data.burstSettings)
	{
		j["burstSettings"] = {
			{"count", data.burstSettings->count},
			{"interval", data.burstSettings->interval},
		};
	}

	// ---------- optional: chargeSettings ----------
	if (data.chargeSettings)
	{
		j["chargeSettings"] = {
			{"maxChargeTime", data.chargeSettings->maxChargeTime},
		};
	}

	// ---------- optional: meleeData ----------
	if (data.meleeData)
	{
		j["meleeData"] = {
			{"attackRange", data.meleeData->attackRange},
			{"attackArc", data.meleeData->attackArc},
			{"verticalAngle", data.meleeData->verticalAngle},

			{"startupDelay", data.meleeData->startupDelay},
			{"activeFrames", data.meleeData->activeFrames},
			{"recoveryDelay", data.meleeData->recoveryDelay},

			{"maxComboCount", data.meleeData->maxComboCount},
			{"comboWindow", data.meleeData->comboWindow},

			{"dashImpulse", data.meleeData->dashImpulse},
			{"bCanCharge", data.meleeData->bCanCharge},
			{"maxChargeTime", data.meleeData->maxChargeTime},
			{"chargeDamageMultiplier", data.meleeData->chargeDamageMultiplier},

			{"bCanBlock", data.meleeData->bCanBlock},
			{"blockDamageReduction", data.meleeData->blockDamageReduction},

			{"hitStopDuration", data.meleeData->hitStopDuration},
			{"knockbackForce", data.meleeData->knockbackForce},
		};
	}

	// ---------- fire mode / flags ----------
	j["defaultFireMode"] = FireModeToStr(data.defaultFireMode);

	j["supportedFireModels"] = json::array();
	for (auto m : data.supportedFireModels)
		j["supportedFireModels"].push_back(FireModeToStr(m));

	j["bIsAutomatic"] = data.bIsAutomatic;
	j["bCanToggleFireMode"] = data.bCanToggleFireMode;

	// ---------- attributes ----------
	j["attributes"] = json::array();
	for (auto a : data.attributes)
	{
		// 文字列保存（loader は数値/文字列どちらも対応）
		j["attributes"].push_back(AttributeToStr(a));
	}

	std::error_code ec;
	std::filesystem::create_directories(filePath.parent_path(), ec);

	std::ofstream ofs(filePath);
	if (!ofs)
	{
		if (outError) *outError = "Failed to open: " + filePath.string();
		return false;
	}

	ofs << j.dump(4);
	return true;
}

std::filesystem::path WeaponMasterDataWriter::MakeWeaponFilePath(const std::filesystem::path& rootDir, const FWeaponMasterData& data)
{
	const auto dir = rootDir / CategoryFolder(data.coreData.category);

	const std::string stem = SanitizeFileStem(data.coreData.weaponName);
	const std::string idStr = std::to_string(data.coreData.weaponID);

	// 末尾が _<id>なら二重付与しない
	const std::string suffix = "_" + idStr;
	const bool already = (stem.size() >= suffix.size()) &&
		(stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0);

	const std::string fileStem = already ? stem : (stem + "_" + idStr);
	return dir / (fileStem + ".json");
}

std::string WeaponMasterDataWriter::SanitizeFileStem(std::string s)
{
	const char* bad = "\\/:*?\"<>|";
	for (char& c : s)
	{
		if ((unsigned char)c < 32) c = '_';
		for (const char* p = bad; *p; ++p)
		{
			if (c == *p) { c = '_'; break; }
		}
	}
	while (!s.empty() && (s.back() == '.' || s.back() == ' ')) s.pop_back();
	if (s.empty()) s = "Weapon";
	return s;
}

/// -------------------------------------------------------------
///		指定したルートディレクトリ内で、指定した武器IDに対応するファイルを削除します
/// -------------------------------------------------------------
bool WeaponMasterDataWriter::DeleteFilesByWeaponID(const std::filesystem::path& rootDir, int32_t weaponID, std::string* outError)
{
	namespace fs = std::filesystem;
	std::error_code ec;

	if (!fs::exists(rootDir, ec))
		return true;

	const std::string legacy = std::to_string(weaponID) + ".json";
	const std::string suffix = "_" + std::to_string(weaponID) + ".json";

	for (auto it = fs::directory_iterator(rootDir, ec); !ec && it != fs::directory_iterator(); it.increment(ec))
	{
		if (!it->is_directory()) continue;

		for (auto fi = fs::directory_iterator(it->path(), ec); !ec && fi != fs::directory_iterator(); fi.increment(ec))
		{
			if (!fi->is_regular_file()) continue;

			const std::string fn = fi->path().filename().string();
			if (fn == legacy || EndsWith(fn, suffix))
			{
				fs::remove(fi->path(), ec);
				if (ec && outError) *outError = "Failed to remove: " + fi->path().string();
			}
		}
	}

	return true;
}

bool WeaponMasterDataWriter::DeleteFilesForWeapon(const std::filesystem::path& rootDir, const FWeaponMasterData& data, std::string* outError)
{
	// まずID系を掃除（name_id.json や 1.json）
	DeleteFilesByWeaponID(rootDir, data.coreData.weaponID, outError);

	// さらに name.json が残るケースを掃除
	namespace fs = std::filesystem;
	std::error_code ec;

	if (!fs::exists(rootDir, ec)) return true;

	const std::string stem = SanitizeFileStem(data.coreData.weaponName);
	const std::string nameOnly = stem + ".json";

	for (auto it = fs::directory_iterator(rootDir, ec);
		!ec && it != fs::directory_iterator();
		it.increment(ec))
	{
		if (!it->is_directory()) continue;

		const fs::path p = it->path() / nameOnly;
		if (fs::exists(p, ec))
		{
			fs::remove(p, ec);
			if (ec && outError) *outError = "Failed to remove: " + p.string();
		}
	}
	return true;
}
