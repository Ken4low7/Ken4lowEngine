#define NOMINMAX
#include "WeaponSystem.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>

#include "WeaponMasterData.h"
#include "WeaponMasterDataValidator.h"

namespace
{
	bool ContainsFireMode(const std::vector<EFireMode>& modes, EFireMode target)
	{
		return std::find(modes.begin(), modes.end(), target) != modes.end();
	}
}

std::filesystem::path WeaponSystem::ResolveWeaponsRoot(const std::filesystem::path& inputPath)
{
	if (inputPath.empty()) return inputPath;

	const auto name = inputPath.filename().string();
	if (name == "weapons") return inputPath;
	if (name == "JSON" || name == "json") return inputPath / "weapons";
	return inputPath;
}

bool WeaponSystem::Load(const std::filesystem::path& rootDir, std::string* outError)
{
	equippedWeaponId_ = 0;
	db_.Clear();

	const auto weaponsRoot = ResolveWeaponsRoot(rootDir);

	if (!db_.LoadFromDirectory(weaponsRoot, outError))
	{
		return false;
	}
	return true;
}

bool WeaponSystem::ReloadAndReequip(std::string* outError)
{
	if (!db_.IsLoaded())
	{
		if (outError) *outError = "WeaponSystem: database is not loaded.";
		return false;
	}

	const int32_t prev = equippedWeaponId_;

	if (!db_.Reload(outError))
	{
		return false;
	}

	if (prev > 0 && db_.ContainsID(prev))
	{
		return EquipById(prev, outError);
	}

	equippedWeaponId_ = 0;
	if (db_.Size() == 0)
	{
		if (outError) outError->clear();
		return true;
	}
	return EquipFirst(outError);
}

bool WeaponSystem::RebuildEquippedFromDatabase(std::string* outError)
{
	if (equippedWeaponId_ <= 0)
	{
		if (outError) *outError = "WeaponSystem: no weapon is currently equipped.";
		return false;
	}
	return EquipById(equippedWeaponId_, outError);
}

bool WeaponSystem::EquipFirst(std::string* outError)
{
	std::vector<int32_t> ids;
	if (!TryGetSortedWeaponIds(ids, outError)) { return false; }
	return EquipById(ids.front(), outError);
}

bool WeaponSystem::EquipById(int32_t weaponId, std::string* outError)
{
	const FWeaponMasterData* src = db_.FindByID(weaponId);
	if (!src)
	{
		if (outError) *outError = "WeaponSystem: weaponId not found.";
		return false;
	}

	// エディタ編集中の不正値がそのままランタイムへ入らないよう、適用前に正規化と検証を行う
	FWeaponMasterData md = *src;
	WeaponMasterDataDatabase::NormalizeByCategory(md);

	std::string validateErr;
	if (!WeaponMasterDataValidator::Validate(md, &validateErr))
	{
		if (outError)
		{
			*outError = "WeaponSystem: invalid master data for equip (weaponId=" +
				std::to_string(weaponId) + ")\n" + validateErr;
		}
		return false;
	}

	WeaponParams p = BuildParams(md);
	weapon_.Equip(p);

	// HUDが装備中武器の照準画像や拡散表示を参照できるよう、レティクル設定を保持する
	equippedReticleData_ = md.reticleData;

	equippedWeaponId_ = weaponId;
	return true;
}

bool WeaponSystem::EquipNext(std::string* outError)
{
	return EquipRelative(1, outError);
}

bool WeaponSystem::EquipPrev(std::string* outError)
{
	return EquipRelative(-1, outError);
}

bool WeaponSystem::EquipRelative(int direction, std::string* outError)
{
	std::vector<int32_t> ids;
	if (!TryGetSortedWeaponIds(ids, outError)) { return false; }

	auto it = std::find(ids.begin(), ids.end(), equippedWeaponId_);
	if (it == ids.end() || equippedWeaponId_ <= 0)
	{
		return EquipById(direction > 0 ? ids.front() : ids.back(), outError);
	}

	// 武器一覧の端を越えた場合は反対側へ循環させる。
	const auto currentIndex = static_cast<std::ptrdiff_t>(std::distance(ids.begin(), it));
	const auto count = static_cast<std::ptrdiff_t>(ids.size());
	const auto nextIndex = (currentIndex + direction + count) % count;
	return EquipById(ids[static_cast<size_t>(nextIndex)], outError);
}

bool WeaponSystem::TryGetSortedWeaponIds(std::vector<int32_t>& ids, std::string* outError) const
{
	if (!db_.IsLoaded() || db_.Size() == 0)
	{
		if (outError) { *outError = "WeaponSystem: database is not loaded or empty."; }
		return false;
	}

	ids = db_.GetSortedIDList();
	if (ids.empty())
	{
		if (outError) { *outError = "WeaponSystem: ID list is empty."; }
		return false;
	}
	return true;
}

WeaponParams WeaponSystem::BuildParams(const FWeaponMasterData& md)
{
	WeaponParams p{};

	p.weaponID = md.coreData.weaponID;
	p.weaponCategory = md.coreData.category;
	p.deathKnockbackType = md.deathReaction.type;
	p.deathKnockbackPower = md.deathReaction.power;
	p.deathKnockbackUpPower = md.deathReaction.upPower;
	p.deathExplosionRadius = md.deathReaction.explosionRadius;
	p.deathImpulseScale = md.deathReaction.impulseScale;
	p.isAutomatic = md.bIsAutomatic;
	p.canToggleFireMode = md.bCanToggleFireMode;
	p.drawProjectileModel = (md.coreData.category == EWeaponCategory::Heavy);

	// MasterDataの編集値を、実行時に扱いやすいWeaponParamsへ詰め替える
	p.damage = md.stats.damage;

	const float rpm = md.stats.fireRate;
	p.secPerShot = (rpm > 1e-3f) ? (60.0f / rpm) : 0.1f;

	p.magCapacity = std::max(1, md.stats.capacity);
	p.ammoPerShot = std::max(1, md.stats.ammoPerShot);
	p.maxReserveAmmo = std::max(0, md.stats.maxReserveAmmo);

	// リロード時間は通常/残弾あり/空マガジンを分け、WeaponInstance側で状態に応じて選択する
	p.reloadSec = std::max(0.0f, md.stats.reloadTime);
	p.tacticalReloadSec = std::max(0.0f, md.stats.tacticalReloadTime);
	p.emptyReloadSec = std::max(0.0f, md.stats.emptyReloadTime);
	p.canInterruptReload = md.stats.bCanInterruptReload;

	// ペレット数を1以上に丸め、通常弾と散弾を同じ発射処理で扱えるようにする
	p.pelletCount = std::max(1, md.stats.pelletCount);
	p.pelletSpreadAngle = std::max(0.0f, md.stats.pelletSpreadAngle);

	// 拡散・反動・ADSに関わる操作感の調整値を反映する
	p.accuracy = md.handling.accuracy;
	p.spreadIncrease = std::max(0.0f, md.handling.spreadIncrease);
	p.recoilRecovery = std::max(0.0f, md.handling.recoilRecovery);

	// 実際の弾方向計算で使う散布界は度数として扱い、負値を入れないようにする
	p.baseHipSpreadDeg = std::max(0.0f, md.handling.baseHipSpread);
	p.baseAdsSpreadDeg = std::max(0.0f, md.handling.baseAdsSpread);
	p.spreadRecoveryRate = std::max(0.0f, md.handling.spreadRecoveryRate);
	p.maxSpreadDeg = std::max(0.0f, md.handling.maxSpread);

	// エイム中の画角、移行速度、移動速度倍率を操作パラメータへ反映する。
	p.adsZoomFov = md.handling.adsZoomFov;
	p.adsTransitionSpeed = md.handling.adsTransitionSpeed;
	p.adsMoveSpeedMultiplier = std::clamp(md.handling.adsMoveSpeedMultiplier, 0.0f, 2.0f);

	// 投射物方式の武器だけ、弾速や寿命、爆発範囲などの追加情報を反映する。
	if (md.projectileData.has_value())
	{
		p.isProjectile = md.projectileData->bIsProjectile;
		p.projectileSpeed = md.projectileData->projectileSpeed;
		p.projectileLifeTime = std::max(0.01f, md.projectileData->projectileLifeTime);
		p.maxRange = (md.projectileData->maxRange > 0.0f) ? md.projectileData->maxRange : p.maxRange;
		p.traceRadius = std::max(0.0f, md.projectileData->traceRadius);
		p.splashRadius = std::max(0.0f, md.projectileData->splashRadius);
		p.splashDamage = (p.splashRadius > 0.0f) ? std::max(1, static_cast<int>(md.stats.damage)) : 0;
		p.splashCanDamageSelf = md.projectileData->bCanDamageSelf;

		// 銃口前方オフセットをデータ化し、武器ごとに弾の発生位置を調整できるようにする
		if (md.projectileData->spawnForwardOffset > 0.0f)
			p.muzzleForwardOffset = md.projectileData->spawnForwardOffset;
	}

	// バースト射撃は2発以上に設定された場合だけ有効化する。
	if (md.burstSettings.has_value() && md.burstSettings->count >= 2)
	{
		p.burstCount = md.burstSettings->count;
		p.burstIntervalSec = std::max(0.0f, md.burstSettings->interval);
	}

	// チャージ設定がある武器だけ最大チャージ時間を使用する。
	if (md.chargeSettings.has_value())
	{
		p.maxChargeTime = std::max(0.0f, md.chargeSettings->maxChargeTime);
	}

	return p;
}
