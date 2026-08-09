#include "WeaponMasterDataLoader.h"
#include "WeaponMasterDataValidator.h"
#include "PathUtil.h"
#include "JsonReadUtil.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include <unordered_set>
#include <system_error>

#include <json.hpp>
#include "WeaponMasterData.h"
using nlohmann::json;

namespace
{
	static bool IsUnderWeaponCategoryFolder(const std::filesystem::path& filePath, const std::filesystem::path& rootDire)
	{
		std::error_code ec;
		if (std::filesystem::equivalent(filePath.parent_path(), rootDire, ec)) return false;

		// カテゴリフォルダ配下のみ許可
		static const std::unordered_set<std::string> kAllowed = { "primary", "backup", "melee", "special", "sniper", "heavy" };

		ec.clear();

		const auto rel = std::filesystem::relative(filePath.parent_path(), rootDire, ec);
		if (ec) return false;

		auto it = rel.begin();
		if (it == rel.end()) return false;

		const std::string top = it->string();

		if (top.size() >= 5 && top.ends_with(".json"))
			return false; // 直下にファイルがある場合

		return kAllowed.contains(top);
	}
}

/// ---------- 小物ユーティリティ ---------- ///
static std::string ToLower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

template<typename T>
static void ReadIfExists(const json& j, const char* key, T& outValue)
{
	if (j.contains(key) && !j.at(key).is_null())
	{
		outValue = j.at(key).get<T>();
	}
}

template<typename T>
static void ReadOptionalIfExists(const json& j, const char* key, std::optional<T>& outOpt)
{
	if (!j.contains(key) || j.at(key).is_null())
	{
		outOpt.reset();
		return;
	}
	outOpt = j.at(key).get<T>();
}

/// ---------- enum 変換（文字列 or 数値 両対応）---------- ///
static bool ParseEnum_EWeaponRarity(const json& v, EWeaponRarity& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EWeaponRarity>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "common") { out = EWeaponRarity::Common; return true; }
	if (s == "rare") { out = EWeaponRarity::Rare; return true; }
	if (s == "epic") { out = EWeaponRarity::Epic; return true; }
	if (s == "legendary") { out = EWeaponRarity::Legendary; return true; }
	if (s == "mythical") { out = EWeaponRarity::Mythical; return true; }
	return false;
}

static bool ParseEnum_EAmmoType(const json& v, EAmmoType& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EAmmoType>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "default") { out = EAmmoType::Default; return true; }
	if (s == "energy") { out = EAmmoType::Energy; return true; }
	if (s == "explosive") { out = EAmmoType::Explosive; return true; }
	if (s == "none") { out = EAmmoType::None; return true; }
	return false;
}

static bool ParseEnum_EWeaponCategory(const json& v, EWeaponCategory& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EWeaponCategory>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "primary") { out = EWeaponCategory::Primary; return true; }
	if (s == "backup") { out = EWeaponCategory::Backup; return true; }
	if (s == "melee") { out = EWeaponCategory::Melee; return true; }
	if (s == "special") { out = EWeaponCategory::Special; return true; }
	if (s == "sniper") { out = EWeaponCategory::Sniper; return true; }
	if (s == "heavy") { out = EWeaponCategory::Heavy; return true; }
	return false;
}

static bool ParseEnum_EWeaponAttribute(const json& v, EWeaponAttribute& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EWeaponAttribute>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "none") { out = EWeaponAttribute::None; return true; }
	if (s == "poison") { out = EWeaponAttribute::Poison; return true; }
	if (s == "burning") { out = EWeaponAttribute::Burning; return true; }
	if (s == "areadamage") { out = EWeaponAttribute::AreaDamage; return true; }
	if (s == "bouncing") { out = EWeaponAttribute::Bouncing; return true; }
	if (s == "lifesteal") { out = EWeaponAttribute::LifeSteal; return true; }
	if (s == "wallbreak") { out = EWeaponAttribute::WallBreak; return true; }
	if (s == "fixeddelay") { out = EWeaponAttribute::FixedDelay; return true; }
	return false;
}

static bool ParseEnum_EFireMode(const json& v, EFireMode& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EFireMode>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "semi" || s == "semiauto") { out = EFireMode::SemiAuto; return true; }
	if (s == "burst") { out = EFireMode::Burst;    return true; }
	if (s == "fullauto" || s == "auto") { out = EFireMode::FullAuto; return true; }
	if (s == "charge") { out = EFireMode::Charge;   return true; }
	return false;
}

static bool ParseEnum_EReticleType(const json& v, EReticleType& out)
{
	if (v.is_number_integer())
	{
		out = static_cast<EReticleType>(v.get<int>());
		return true;
	}
	if (!v.is_string()) return false;

	const std::string s = ToLower(v.get<std::string>());
	if (s == "none") { out = EReticleType::None; return true; }
	if (s == "dot") { out = EReticleType::Dot; return true; }
	if (s == "cross") { out = EReticleType::Cross; return true; }
	if (s == "circle") { out = EReticleType::Circle; return true; }
	if (s == "scope") { out = EReticleType::Scope; return true; }
	return false;
}
static bool ParseEnum_EDeathKnockbackType(const json& v, EDeathKnockbackType& out)
{
	if (v.is_number_integer()) { out = static_cast<EDeathKnockbackType>(v.get<int>()); return true; }
	if (!v.is_string()) return false;
	const std::string s = ToLower(v.get<std::string>());
	if (s == "default") { out = EDeathKnockbackType::Default; return true; }
	if (s == "light") { out = EDeathKnockbackType::Light; return true; }
	if (s == "sniper") { out = EDeathKnockbackType::Sniper; return true; }
	if (s == "heavy") { out = EDeathKnockbackType::Heavy; return true; }
	if (s == "explosion") { out = EDeathKnockbackType::Explosion; return true; }
	return false;
}

template<typename TEnum, typename TParser>
static void ReadEnumIfExists(const json& j, const char* key, TEnum& outValue, TParser& parser)
{
	if (!j.contains(key) || j.at(key).is_null()) return;

	TEnum temp = outValue;
	if (parser(j.at(key), temp)) outValue = temp;
}

template<typename TEnum, typename TParser>
static void ReadEnumArrayIfExists(const json& j, const char* key, std::vector<TEnum>& outValues, TParser& parser)
{
	if (!j.contains(key) || j.at(key).is_null()) return;

	std::vector<TEnum> tempVec;
	for (const auto& item : j.at(key))
	{
		TEnum temp = TEnum{};
		if (parser(item, temp)) tempVec.push_back(temp);
	}
	outValues = std::move(tempVec);
}

/// ---------- struct の from_json 定義 ---------- ///
static void from_json(const json& j, FWeaponCore& v)
{
	ReadIfExists(j, "weaponID", v.weaponID);
	ReadIfExists(j, "weaponName", v.weaponName);

	if (j.contains("category"))
	{
		EWeaponCategory cat = v.category;
		if (ParseEnum_EWeaponCategory(j.at("category"), cat)) v.category = cat;
	}
	if (j.contains("rarity"))
	{
		EWeaponRarity rar = v.rarity;
		if (ParseEnum_EWeaponRarity(j.at("rarity"), rar)) v.rarity = rar;
	}
}

static void from_json(const json& j, FWeaponStats& v)
{
	ReadIfExists(j, "damage", v.damage);
	ReadIfExists(j, "minDamage", v.minDamage);
	ReadIfExists(j, "damageFalloffStart", v.damageFalloffStart);
	ReadIfExists(j, "damageFalloffEnd", v.damageFalloffEnd);

	ReadIfExists(j, "fireRate", v.fireRate);
	ReadIfExists(j, "mobility", v.mobility);
	ReadIfExists(j, "capacity", v.capacity);

	if (j.contains("ammoType"))
	{
		EAmmoType a = v.ammoType;
		if (ParseEnum_EAmmoType(j.at("ammoType"), a)) v.ammoType = a;
	}

	ReadIfExists(j, "reloadTime", v.reloadTime);
	ReadIfExists(j, "tacticalReloadTime", v.tacticalReloadTime);
	ReadIfExists(j, "emptyReloadTime", v.emptyReloadTime);
	ReadIfExists(j, "bReloadByShell", v.bReloadByShell);
	ReadIfExists(j, "bCanInterruptReload", v.bCanInterruptReload);

	ReadIfExists(j, "ammoPerShot", v.ammoPerShot);
	ReadIfExists(j, "maxReserveAmmo", v.maxReserveAmmo);
	ReadIfExists(j, "bHasChamber", v.bHasChamber);
	ReadIfExists(j, "chamberSize", v.chamberSize);

	ReadIfExists(j, "criticalChance", v.criticalChance);
	ReadIfExists(j, "headshotMultiplier", v.headshotMultiplier);
	ReadIfExists(j, "bodyMultiplier", v.bodyMultiplier);
	ReadIfExists(j, "armMultiplier", v.armMultiplier);
	ReadIfExists(j, "legMultiplier", v.legMultiplier);

	ReadIfExists(j, "pelletCount", v.pelletCount);
	ReadIfExists(j, "pelletSpreadAngle", v.pelletSpreadAngle);
}

static void from_json(const json& j, FWeaponHandling& v)
{
	ReadIfExists(j, "accuracy", v.accuracy);
	ReadIfExists(j, "spreadIncrease", v.spreadIncrease);

	ReadIfExists(j, "baseHipSpread", v.baseHipSpread);
	ReadIfExists(j, "baseAdsSpread", v.baseAdsSpread);
	ReadIfExists(j, "moveSpreadMultiplier", v.moveSpreadMultiplier);
	ReadIfExists(j, "jumpSpreadMultiplier", v.jumpSpreadMultiplier);
	ReadIfExists(j, "crouchSpreadMultiplier", v.crouchSpreadMultiplier);
	ReadIfExists(j, "spreadRecoveryRate", v.spreadRecoveryRate);
	ReadIfExists(j, "maxSpread", v.maxSpread);

	ReadIfExists(j, "verticalRecoil", v.verticalRecoil);
	ReadIfExists(j, "horizontalRecoil", v.horizontalRecoil);
	ReadIfExists(j, "cameraRecoilPitch", v.cameraRecoilPitch);
	ReadIfExists(j, "cameraRecoilYaw", v.cameraRecoilYaw);
	ReadIfExists(j, "weaponKickBack", v.weaponKickBack);
	ReadIfExists(j, "recoilRecovery", v.recoilRecovery);
	ReadIfExists(j, "recoilResetDelay", v.recoilResetDelay);

	ReadIfExists(j, "adsZoomFov", v.adsZoomFov);
	ReadIfExists(j, "zoomLevel", v.zoomLevel);
	ReadIfExists(j, "adsTransitionSpeed", v.adsTransitionSpeed);
	ReadIfExists(j, "adsInTime", v.adsInTime);
	ReadIfExists(j, "adsOutTime", v.adsOutTime);
	ReadIfExists(j, "adsMoveSpeedMultiplier", v.adsMoveSpeedMultiplier);

	ReadIfExists(j, "equipTime", v.equipTime);
	ReadIfExists(j, "unequipTime", v.unequipTime);
	ReadIfExists(j, "sprintToFireTime", v.sprintToFireTime);
	ReadIfExists(j, "fireToSprintTime", v.fireToSprintTime);
	ReadIfExists(j, "fixedDelayTime", v.fixedDelayTime);
}

static void from_json(const json& j, FWeaponProjectileData& v)
{
	ReadIfExists(j, "maxRange", v.maxRange);
	ReadIfExists(j, "bIsProjectile", v.bIsProjectile);
	ReadIfExists(j, "projectileSpeed", v.projectileSpeed);

	ReadIfExists(j, "gravityScale", v.gravityScale);
	ReadIfExists(j, "projectileLifeTime", v.projectileLifeTime);
	ReadIfExists(j, "projectileDrag", v.projectileDrag);
	ReadIfExists(j, "projectileRadius", v.projectileRadius);
	ReadIfExists(j, "traceRadius", v.traceRadius);
	ReadIfExists(j, "spawnForwardOffset", v.spawnForwardOffset);

	ReadIfExists(j, "pierceCount", v.pierceCount);
	ReadIfExists(j, "ricochetCount", v.ricochetCount);
	ReadIfExists(j, "splashRadius", v.splashRadius);
	ReadIfExists(j, "bCanDamageSelf", v.bCanDamageSelf);
}

static void from_json(const json& j, FBurstSettings& v)
{
	ReadIfExists(j, "count", v.count);
	ReadIfExists(j, "interval", v.interval);
}

static void from_json(const json& j, FChargeSettings& v)
{
	ReadIfExists(j, "maxChargeTime", v.maxChargeTime);
}

static void from_json(const json& j, FMeleeWeaponData& v)
{
	ReadIfExists(j, "attackRange", v.attackRange);
	ReadIfExists(j, "attackArc", v.attackArc);
	ReadIfExists(j, "verticalAngle", v.verticalAngle);

	ReadIfExists(j, "startupDelay", v.startupDelay);
	ReadIfExists(j, "activeFrames", v.activeFrames);
	ReadIfExists(j, "recoveryDelay", v.recoveryDelay);

	ReadIfExists(j, "maxComboCount", v.maxComboCount);
	ReadIfExists(j, "comboWindow", v.comboWindow);

	ReadIfExists(j, "dashImpulse", v.dashImpulse);
	ReadIfExists(j, "bCanCharge", v.bCanCharge);
	ReadIfExists(j, "maxChargeTime", v.maxChargeTime);
	ReadIfExists(j, "chargeDamageMultiplier", v.chargeDamageMultiplier);

	// ここが元コードで抜けていた分
	ReadIfExists(j, "bCanBlock", v.bCanBlock);
	ReadIfExists(j, "blockDamageReduction", v.blockDamageReduction);

	ReadIfExists(j, "hitStopDuration", v.hitStopDuration);
	ReadIfExists(j, "knockbackForce", v.knockbackForce);
}

static void from_json(const json& j, FWeaponReticleData& v)
{
	// 既存互換
	ReadIfExists(j, "reticleTexturePath", v.reticleTexturePath);

	// 基本
	ReadIfExists(j, "reticleBaseSize", v.reticleBaseSize);
	ReadIfExists(j, "reticleMaxSize", v.reticleMaxSize);
	ReadIfExists(j, "reticleExpandPerShot", v.reticleExpandPerShot);
	ReadIfExists(j, "reticleRecoverSpeed", v.reticleRecoverSpeed);

	// 移動拡散
	ReadIfExists(j, "bEnableMoveReticleExpand", v.bEnableMoveReticleExpand);
	ReadIfExists(j, "moveExpandMultiplier", v.moveExpandMultiplier);
	ReadIfExists(j, "sprintExpandMultiplier", v.sprintExpandMultiplier);
	ReadIfExists(j, "airExpandMultiplier", v.airExpandMultiplier);
	ReadIfExists(j, "landExpandImpulse", v.landExpandImpulse);

	// ADS
	ReadIfExists(j, "bHideReticleInADS", v.bHideReticleInADS);
	ReadIfExists(j, "bUseAdsReticleOverride", v.bUseAdsReticleOverride);
	ReadIfExists(j, "adsReticleTexturePath", v.adsReticleTexturePath);
	ReadIfExists(j, "bUseAdsCenterDot", v.bUseAdsCenterDot);
	ReadIfExists(j, "adsCenterDotTexturePath", v.adsCenterDotTexturePath);
	ReadIfExists(j, "adsReticleBlendTime", v.adsReticleBlendTime);

	// ヒット / 撃破
	ReadIfExists(j, "bShowHitMarker", v.bShowHitMarker);
	ReadIfExists(j, "hitMarkerTexturePath", v.hitMarkerTexturePath);
	ReadIfExists(j, "bUseHeadshotMarker", v.bUseHeadshotMarker);
	ReadIfExists(j, "headshotHitMarkerTexturePath", v.headshotHitMarkerTexturePath);
	ReadIfExists(j, "bUseKillConfirmMarker", v.bUseKillConfirmMarker);
	ReadIfExists(j, "killConfirmMarkerTexturePath", v.killConfirmMarkerTexturePath);
	ReadIfExists(j, "hitMarkerDuration", v.hitMarkerDuration);
	ReadIfExists(j, "killConfirmDuration", v.killConfirmDuration);

	if (j.contains("reticleType"))
	{
		EReticleType t = v.reticleType;
		if (ParseEnum_EReticleType(j.at("reticleType"), t))
			v.reticleType = t;
	}
}

static void from_json(const json& j, FWeaponVfx& v)
{
	ReadIfExists(j, "muzzleFlashVfxPath", v.muzzleFlashVfxPath);
	ReadIfExists(j, "tracerVfxPath", v.tracerVfxPath);
	ReadIfExists(j, "impactVfxPath", v.impactVfxPath);
	ReadIfExists(j, "shellEjectVfxPath", v.shellEjectVfxPath);

	ReadIfExists(j, "reloadVfxPath", v.reloadVfxPath);
	ReadIfExists(j, "chargeVfxPath", v.chargeVfxPath);

	ReadIfExists(j, "meleeSwingVfxPath", v.meleeSwingVfxPath);
	ReadIfExists(j, "meleeHitVfxPath", v.meleeHitVfxPath);

	ReadIfExists(j, "muzzleFlashScale", v.muzzleFlashScale);
	ReadIfExists(j, "tracerScale", v.tracerScale);
	ReadIfExists(j, "impactScale", v.impactScale);
	ReadIfExists(j, "meleeSwingScale", v.meleeSwingScale);
	ReadIfExists(j, "meleeHitScale", v.meleeHitScale);
}

static void from_json(const json& j, FWeaponSockets& v)
{
	ReadIfExists(j, "weaponAttachSocket", v.weaponAttachSocket);
	ReadIfExists(j, "rightHandSocket", v.rightHandSocket);
	ReadIfExists(j, "leftHandIkSocket", v.leftHandIkSocket);
	ReadIfExists(j, "adsCameraSocket", v.adsCameraSocket);
	ReadIfExists(j, "magazineSocket", v.magazineSocket);

	ReadIfExists(j, "muzzleSocket", v.muzzleSocket);
	ReadIfExists(j, "shellEjectSocket", v.shellEjectSocket);
	ReadIfExists(j, "tracerStartSocket", v.tracerStartSocket);
	ReadIfExists(j, "scopeSocket", v.scopeSocket);

	ReadIfExists(j, "meleeTraceStartSocket", v.meleeTraceStartSocket);
	ReadIfExists(j, "meleeTraceEndSocket", v.meleeTraceEndSocket);
	ReadIfExists(j, "meleeHitSocket", v.meleeHitSocket);
}

static void from_json(const json& j, FWeaponEconomyData& v)
{
	// EconomyDataの単純値だけJsonReadUtilへ寄せる。既存キー名とstruct既定値はそのまま維持する。
	v.currencyType = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "currencyType", v.currencyType);
	v.purchasePrice = Ken4lowEngine::JsonReadUtil::ReadIntOr(j, "purchasePrice", v.purchasePrice);
	v.minLevelToUnlock = Ken4lowEngine::JsonReadUtil::ReadIntOr(j, "minLevelToUnlock", v.minLevelToUnlock);
	// upgradeCostsは配列読み取りのため、今回の単純値共通化では触らず既存処理を残す。
	ReadIfExists(j, "upgradeCosts", v.upgradeCosts);
	v.bIsLimitedTime = Ken4lowEngine::JsonReadUtil::ReadBoolOr(j, "bIsLimitedTime", v.bIsLimitedTime);
	v.discountRate = Ken4lowEngine::JsonReadUtil::ReadFloatOr(j, "discountRate", v.discountRate);
}

static void from_json(const json& j, FWeaponAssets& v)
{
	// Asset系はstringパスだけJsonReadUtilへ寄せる。既存キー名と既定値を維持し、数値・enum・旧キー互換には触れない。
	v.modelPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "modelPath", v.modelPath);
	v.iconPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "iconPath", v.iconPath);

	// 旧データが "Resources/Models/..." でも内部表現は "Sources/..." に揃える
	v.modelPath = K4E::PathUtil::ToModelRelativePath(v.modelPath);
}

static void from_json(const json& j, FWeaponSounds& v)
{
	// Sound系はstring読み取りだけを共通化し、サウンドキー名と既定値は既存Json互換のため変えない。
	v.fireSoundPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "fireSoundPath", v.fireSoundPath);
	v.reloadSoundPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "reloadSoundPath", v.reloadSoundPath);
	v.emptySoundPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "emptySoundPath", v.emptySoundPath);
	v.equipSoundPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "equipSoundPath", v.equipSoundPath);
	v.impactSoundPath = Ken4lowEngine::JsonReadUtil::ReadStringOr(j, "impactSoundPath", v.impactSoundPath);
}
static void from_json(const json& j, FWeaponDeathReaction& v)
{
	ReadEnumIfExists(j, "type", v.type, ParseEnum_EDeathKnockbackType);
	ReadIfExists(j, "power", v.power);
	ReadIfExists(j, "upPower", v.upPower);
	ReadIfExists(j, "explosionRadius", v.explosionRadius);
	ReadIfExists(j, "impulseScale", v.impulseScale);
}

static void from_json(const json& j, FWeaponMasterData& v)
{
	// struct側のデフォルト値を活かしたいので「存在する物だけ上書き」方式
	if (j.contains("coreData"))     v.coreData = j.at("coreData").get<FWeaponCore>();
	if (j.contains("economyData"))  v.economyData = j.at("economyData").get<FWeaponEconomyData>();

	if (j.contains("stats"))        v.stats = j.at("stats").get<FWeaponStats>();
	if (j.contains("handling"))     v.handling = j.at("handling").get<FWeaponHandling>();
	if (j.contains("reticleData"))  v.reticleData = j.at("reticleData").get<FWeaponReticleData>();

	if (j.contains("assetData"))    v.assetData = j.at("assetData").get<FWeaponAssets>();
	if (j.contains("soundData"))    v.soundData = j.at("soundData").get<FWeaponSounds>();
	if (j.contains("vfxData"))      v.vfxData = j.at("vfxData").get<FWeaponVfx>();
	if (j.contains("deathReaction")) v.deathReaction = j.at("deathReaction").get<FWeaponDeathReaction>();
	if (j.contains("socketData"))   v.socketData = j.at("socketData").get<FWeaponSockets>();

	ReadOptionalIfExists(j, "meleeData", v.meleeData);
	ReadOptionalIfExists(j, "projectileData", v.projectileData);
	ReadOptionalIfExists(j, "burstSettings", v.burstSettings);
	ReadOptionalIfExists(j, "chargeSettings", v.chargeSettings);

	ReadIfExists(j, "bIsAutomatic", v.bIsAutomatic);
	ReadIfExists(j, "bCanToggleFireMode", v.bCanToggleFireMode);

	// defaultFireMode
	ReadEnumIfExists(j, "defaultFireMode", v.defaultFireMode, ParseEnum_EFireMode);

	// supportedFireModes（正しい想定キー）
	ReadEnumArrayIfExists(j, "supportedFireModes", v.supportedFireModels, ParseEnum_EFireMode);

	// 互換: 旧/誤字キー（supportedFireModels）も読む
	if (v.supportedFireModels.empty())
	{
		ReadEnumArrayIfExists(j, "supportedFireModels", v.supportedFireModels, ParseEnum_EFireMode);
	}

	// supportedFireModels が空なら default を1つ入れておく（安全策）
	if (v.supportedFireModels.empty())
	{
		v.supportedFireModels.push_back(v.defaultFireMode);
	}

	// ✅ 整合性：オートONなら FullAuto を補完
	if (v.bIsAutomatic)
	{
		const bool hasFull =
			(v.defaultFireMode == EFireMode::FullAuto) ||
			(std::find(v.supportedFireModels.begin(), v.supportedFireModels.end(), EFireMode::FullAuto) != v.supportedFireModels.end());

		if (!hasFull)
			v.supportedFireModels.push_back(EFireMode::FullAuto);
	}

	// attributes は文字列配列 or 数値配列対応
	if (j.contains("attributes") && j.at("attributes").is_array())
	{
		v.attributes.clear();
		for (const auto& a : j.at("attributes"))
		{
			EWeaponAttribute attr = EWeaponAttribute::None;
			if (ParseEnum_EWeaponAttribute(a, attr))
			{
				if (attr != EWeaponAttribute::None)
				{
					v.attributes.push_back(attr);
				}
			}
		}
	}
}

/// -------------------------------------------------------------
///		指定したJSONファイルから武器マスターデータを読み込み
/// -------------------------------------------------------------
bool WeaponMasterDataLoader::LoadFromFile(const std::filesystem::path& jsonPath, FWeaponMasterData& out, std::string* outError)
{
	std::ifstream ifs(jsonPath);
	if (!ifs)
	{
		return Fail(outError, "WeaponMasterDataLoader : ファイルを開けません: " + jsonPath.string());
	}

	json j;
	try
	{
		ifs >> j;
	}
	catch (const std::exception& e)
	{
		return Fail(outError, std::string("WeaponMasterDataLoader: JSON parse 失敗: ") + e.what());
	}

	try
	{
		// ここで from_json が走る
		out = j.get<FWeaponMasterData>();
	}
	catch (const std::exception& e)
	{
		return Fail(outError, std::string("WeaponMasterDataLoader: JSON->Struct変換失敗: ") + e.what());
	}

	std::string validateError;
	if (!WeaponMasterDataValidator::Validate(out, &validateError)) return Fail(outError, validateError);

	return true;
}

/// -------------------------------------------------------------
///	指定したディレクトリからすべての武器マスターデータを読み込み
/// -------------------------------------------------------------
bool WeaponMasterDataLoader::LoadAllFromDirectory(const std::filesystem::path& dirPath, std::unordered_map<int32_t, FWeaponMasterData>& outMap, std::string* outError)
{
	if (!std::filesystem::exists(dirPath))
	{
		return Fail(outError, "WeaponMasterDataLoader: ディレクトリが存在しません: " + dirPath.string());
	}

	outMap.clear();

	std::error_code ec;
	for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath, ec))
	{
		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".json") continue;

		if (!IsUnderWeaponCategoryFolder(entry.path(), dirPath)) continue;

		if (ec)
		{
			return Fail(outError, "WeaponMasterDataLoader: ディレクトリ走査失敗: " + dirPath.string());
		}

		if (!entry.is_regular_file()) continue;
		if (entry.path().extension() != ".json") continue;

		FWeaponMasterData data;
		std::string err;
		if (!LoadFromFile(entry.path(), data, &err))
		{
			return Fail(outError, err + "(file: " + entry.path().string() + ")");
		}

		const int32_t id = data.coreData.weaponID;
		if (outMap.contains(id))
		{
			return Fail(outError, "WeaponMasterDataLoader: weaponIDが重複しています: " + std::to_string(id));
		}

		outMap.emplace(id, std::move(data));
	}

	return true;
}

/// -------------------------------------------------------------
///		エラーメッセージを設定し、失敗を示す false を返す
/// -------------------------------------------------------------
bool WeaponMasterDataLoader::Fail(std::string* outError, std::string_view msg)
{
	if (outError) *outError = std::string(msg);
	return false;
}
