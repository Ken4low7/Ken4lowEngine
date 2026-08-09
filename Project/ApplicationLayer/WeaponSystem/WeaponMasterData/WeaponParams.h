#pragma once

#include <cstdint>
#include "WeaponMasterData.h"

/// <summary>
/// FWeaponMasterDataから生成する、ランタイム武器処理用の固定パラメータ
/// 残弾・クールダウン・リロード中などゲーム中に変化する値はWeaponRuntimeState側に分離する
/// </summary>
struct WeaponParams
{
	// コンストラクタ
	WeaponParams() = default;

	// 武器ID、カテゴリ、撃破時リアクションなど、他システムとの連携に使う識別情報
	int32_t weaponID = 0;
	EWeaponCategory weaponCategory = EWeaponCategory::Primary;
	EDeathKnockbackType deathKnockbackType = EDeathKnockbackType::Default;
	float deathKnockbackPower = 8.0f;
	float deathKnockbackUpPower = 2.0f;
	float deathExplosionRadius = 0.0f;
	float deathImpulseScale = 1.0f;

	// 入力処理やUIに関わる基本挙動。
	bool    isAutomatic = false;
	bool    canToggleFireMode = true;

	// 弾本体のObject3D描画。通常武器はトレーサーだけ見せ、Heavyだけ弾本体を見せる
	bool    drawProjectileModel = false;

	// ダメージ、発射間隔、弾薬数など、射撃結果に直結する値
	float   damage = 10.0f;
	float   secPerShot = 0.1f;

	int32_t magCapacity = 30;
	int32_t ammoPerShot = 1;
	int32_t maxReserveAmmo = 90;

	// リロード（共通 + 詳細）
	float   reloadSec = 1.6f;            // フォールバック
	float   tacticalReloadSec = 1.6f;    // 残弾あり
	float   emptyReloadSec = 2.0f;       // 空マガ時
	bool    canInterruptReload = true;

	// 弾生成と着弾判定に使う値。現在はProjectile運用を主軸にしている
	bool    isProjectile = true;
	float   projectileSpeed = 90.0f;
	float   projectileLifeTime = 3.0f;
	float   maxRange = 100.0f;
	float   traceRadius = 0.0f;          // hitscan方式へ拡張する場合に使う判定半径
	float   muzzleForwardOffset = 0.35f; // projectileData.spawnForwardOffset を反映

	// 着弾時の範囲ダメージ。splashRadius <= 0 なら通常弾。
	float   splashRadius = 0.0f;
	int32_t splashDamage = 0;
	bool    splashCanDamageSelf = false;

	// バースト射撃やチャージ武器など、発射タイミングを変える特殊設定
	int32_t burstCount = 0;
	float   burstIntervalSec = 0.0f;
	float   maxChargeTime = 0.0f;

	// 射撃時の拡散やADS時の操作感に関わる調整値
	float accuracy = 0.8f;
	float spreadIncrease = 0.01f;

	// 散布界
	float baseHipSpreadDeg = 1.0f;
	float baseAdsSpreadDeg = 0.25f;
	float spreadRecoveryRate = 8.0f; // 度/秒
	float maxSpreadDeg = 6.0f;

	// 既存の反動回復処理と接続するために保持する
	float recoilRecovery = 8.0f;

	// ADS時の視野角、移行速度、移動速度倍率
	float adsZoomFov = 60.0f;
	float adsTransitionSpeed = 10.0f;
	float adsMoveSpeedMultiplier = 0.85f; // ADS中の移動倍率

	// ショットガン系の散弾表現に使うペレット設定
	int32_t pelletCount = 1;
	float   pelletSpreadAngle = 0.0f; // ペレット用の追加拡散（度）
};
