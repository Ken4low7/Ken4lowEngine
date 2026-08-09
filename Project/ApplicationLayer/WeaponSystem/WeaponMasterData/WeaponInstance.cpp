#define NOMINMAX
#include "WeaponInstance.h"

#include <algorithm>
#include <cmath>
#include <random>

#include "BulletManager.h"
#include "Camera.h"
#include "CollisionTypeIdDef.h"
#include "Vector3.h"

using namespace Ken4lowEngine;

static float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }


// 射撃方向を円錐内で面積一様にばらつかせ、画面中心からの拡散を自然に見せる
static K4E::Vector3 ApplySpread(const K4E::Vector3& forward, float spreadDeg)
{
	if (spreadDeg <= 0.0f) return Vector3::Normalize(forward);

	static thread_local std::mt19937 rng{ 12345u };
	std::uniform_real_distribution<float> u01(0.0f, 1.0f);

	const float spreadRad = spreadDeg * std::numbers::pi_v<float> / 180.0f;

	// cos(phi)を一様に取ることで、円錐内の弾方向が中心に偏りすぎないようにする
	const float theta = 2.0f * std::numbers::pi_v<float> *u01(rng);
	const float cosPhi = 1.0f - u01(rng) * (1.0f - std::cos(spreadRad));
	const float sinPhi = std::sqrt(std::max(0.0f, 1.0f - cosPhi * cosPhi));

	// forwardを基準に直交基底を作り、カメラ方向に対して自然な散布方向を組み立てる
	K4E::Vector3 f = Vector3::Normalize(forward);
	K4E::Vector3 up = { 0, 1, 0 };
	if (std::fabs(Vector3::Dot(f, up)) > 0.98f) up = { 1, 0, 0 };
	K4E::Vector3 right = Vector3::Normalize(Vector3::Cross(up, f));
	K4E::Vector3 realUp = Vector3::Cross(f, right);

	// 基準方向と横方向成分を合成し、最終的な弾の進行方向にする
	K4E::Vector3 dir = Vector3::Add(
		Vector3::Add(Vector3::Multiply(right, std::cos(theta) * sinPhi), Vector3::Multiply(realUp, std::sin(theta) * sinPhi)),
		Vector3::Multiply(f, cosPhi)
	);
	return Vector3::Normalize(dir);
}

void WeaponInstance::Equip(const WeaponParams& p)
{
	params_ = p;
	fireModeAutomatic_ = params_.isAutomatic;

	st_.magAmmo = params_.magCapacity;
	st_.reserveAmmo = params_.maxReserveAmmo;

	st_.fireCooldown = 0.0f;
	st_.isReloading = false;
	st_.reloadRequested = false;
	st_.reloadRequest = false;
	st_.pendingReload = false;
	st_.reloadTimer = 0.0f;

	st_.burstRemaining = 0;
	st_.burstTimer = 0.0f;

	st_.spread = 0.0f;
	st_.isADS = false;
}

bool WeaponInstance::ToggleFireMode()
{
	if (!params_.canToggleFireMode) return false;
	fireModeAutomatic_ = !fireModeAutomatic_;
	return true;
}

float WeaponInstance::GetCurrentReloadDurationSec() const
{
	// 空マガジンと残弾ありでリロード時間を分け、武器ごとの操作感を出す
	if (st_.magAmmo <= 0 && params_.emptyReloadSec > 0.0f)
		return params_.emptyReloadSec;

	if (params_.tacticalReloadSec > 0.0f)
		return params_.tacticalReloadSec;

	return params_.reloadSec;
}

int32_t WeaponInstance::AddReserveAmmo(int32_t amount)
{
	if (amount <= 0) return 0;

	const int32_t maxReserveAmmo = std::max<int32_t>(0, params_.maxReserveAmmo);
	if (maxReserveAmmo <= 0) return 0;

	const int32_t before = st_.reserveAmmo;
	// AmmoSmallはマガジンではなく予備弾薬へ補充し、リロード処理で装填させる
	st_.reserveAmmo = std::min<int32_t>(maxReserveAmmo, st_.reserveAmmo + amount);
	return st_.reserveAmmo - before;
}

void WeaponInstance::Tick(float dt)
{
	if (st_.reloadRequest)
	{
		st_.reloadRequested = true;
		st_.reloadRequest = false;
	}

	if (st_.isReloading)
	{
		st_.reloadTimer -= dt;
		if (st_.reloadTimer <= 0.0f)
		{
			FinishReload();
			st_.reloadRequested = false;
			st_.reloadTimer = 0.0f;
		}
		return;
	}

	if (st_.reloadRequested || st_.pendingReload)
	{
		if (CanStartReload())
		{
			StartReloadInternal();
		}
		else
		{
			const bool impossible = (st_.magAmmo >= params_.magCapacity) || (st_.reserveAmmo <= 0);
			if (impossible)
			{
				st_.reloadRequested = false;
				st_.pendingReload = false;
			}
			else
			{
				st_.pendingReload = true;
				st_.reloadRequested = false;
			}
		}
	}

	if (st_.fireCooldown > 0.0f) st_.fireCooldown -= dt;

	if (st_.burstRemaining > 0)
	{
		st_.burstTimer -= dt;
		if (st_.burstTimer <= 0.0f)
		{
			st_.fireCooldown = 0.0f;
		}
	}

	// 射撃で広がった照準を時間経過で戻し、連射後に徐々に精度が回復するようにする
	st_.spread = std::max(0.0f, st_.spread - dt * std::max(0.0f, params_.spreadRecoveryRate));
}

void WeaponInstance::StartReload()
{
	if (st_.isReloading) return;
	if (st_.magAmmo >= params_.magCapacity) return;
	if (st_.reserveAmmo <= 0) return;

	st_.isReloading = true;
	st_.reloadTimer = std::max(0.0f, GetCurrentReloadDurationSec());
}

void WeaponInstance::CancelReload()
{
	// 武器ごとの中断可否を守り、リロード演出やバランスを崩さないようにする
	if (!params_.canInterruptReload) return;

	st_.isReloading = false;
	st_.reloadTimer = 0.0f;

	st_.reloadRequested = false;
	st_.reloadRequest = false;
	st_.pendingReload = false;
}

bool WeaponInstance::CanStartReload() const
{
	if (st_.isReloading) return false;
	if (st_.magAmmo >= params_.magCapacity) return false;
	if (st_.reserveAmmo <= 0) return false;
	return true;
}

void WeaponInstance::StartReloadInternal()
{
	st_.isReloading = true;
	st_.reloadTimer = std::max(0.0f, GetCurrentReloadDurationSec());

	st_.reloadRequest = false;
	st_.reloadRequested = false;
	st_.pendingReload = false;
}

bool WeaponInstance::WantFire(bool fireHeld, bool firePressed) const
{
	return fireModeAutomatic_ ? fireHeld : firePressed;
}

bool WeaponInstance::CanFire() const
{
	if (st_.isReloading) return false;
	if (st_.fireCooldown > 0.0f) return false;
	if (st_.magAmmo < params_.ammoPerShot) return false;

	// バースト中は射撃間隔を守るため、次弾の予約時間が終わるまで発射を止める
	if (st_.burstRemaining > 0 && st_.burstTimer > 0.0f) return false;

	return true;
}

void WeaponInstance::ConsumeAmmo()
{
	st_.magAmmo -= params_.ammoPerShot;
	if (st_.magAmmo < 0) st_.magAmmo = 0;
}

void WeaponInstance::FinishReload()
{
	st_.isReloading = false;

	const int need = params_.magCapacity - st_.magAmmo;
	const int add = std::min(need, st_.reserveAmmo);

	st_.magAmmo += add;
	st_.reserveAmmo -= add;
}

void WeaponInstance::TryFire(bool fireHeld, bool firePressed, K4E::Camera* cam, BulletManager* bulletMgr, [[maybe_unused]] CollisionManager* colMgr)
{
	const bool want = WantFire(fireHeld, firePressed);
	if (!want) return;

	if (params_.burstCount >= 2 && st_.burstRemaining == 0)
	{
		st_.burstRemaining = params_.burstCount;
		st_.burstTimer = 0.0f;
	}

	if (!CanFire())
	{
		return;
	}

	ConsumeAmmo();

	// 連射時の拡散増加は基礎拡散を除いた動的分だけに制限する
	const float baseSpread = GetBaseSpreadDeg();
	const float maxDynamic = std::max(0.0f, params_.maxSpreadDeg - baseSpread);
	st_.spread = std::min(maxDynamic, st_.spread + std::max(0.0f, params_.spreadIncrease));

	FireShot(cam, bulletMgr, nullptr);

	if (st_.burstRemaining > 0)
	{
		st_.burstRemaining--;
		if (st_.burstRemaining > 0)
		{
			st_.burstTimer = std::max(0.0f, params_.burstIntervalSec);
			st_.fireCooldown = std::max(0.0f, params_.burstIntervalSec);
		}
		else
		{
			st_.fireCooldown = std::max(0.0f, params_.secPerShot);
		}
	}
	else
	{
		st_.fireCooldown = std::max(0.0f, params_.secPerShot);
	}
}

void WeaponInstance::FireShot(K4E::Camera* cam, BulletManager* bulletMgr, [[maybe_unused]] CollisionManager* colMgr)
{
	if (!cam) return;

	// 現在の実装では弾生成方式をProjectileに統一しているため、非Projectile武器はここでは発射しない
	if (!params_.isProjectile) return;
	if (!bulletMgr) return;

	K4E::Vector3 origin = cam->GetTranslate();
	K4E::Vector3 fwd = Vector3::Normalize(cam->GetForward());

	origin = Vector3::Add(origin, Vector3::Multiply(fwd, params_.muzzleForwardOffset));

	// MasterData由来の基礎拡散と実行時拡散を合成し、武器ごとの精度差を発射方向へ反映する
	const float baseSpreadDeg = GetBaseSpreadDeg();
	const float totalSpreadDeg = std::min(params_.maxSpreadDeg, baseSpreadDeg + st_.spread);

	// 通常弾は1回、ショットガン系はペレット数ぶん弾を生成する
	const int pelletCount = std::max(1, params_.pelletCount);
	const float pelletExtraSpread = std::max(0.0f, params_.pelletSpreadAngle);

	for (int i = 0; i < pelletCount; ++i)
	{
		// ペレット武器だけ追加拡散を乗せ、単発武器と散弾武器の撃ち味を分ける
		const float spreadDeg = totalSpreadDeg + ((pelletCount > 1) ? pelletExtraSpread : 0.0f);
		K4E::Vector3 dir = ApplySpread(fwd, spreadDeg);

		bulletMgr->Spawn(
			origin,
			dir,
			params_.projectileSpeed,
			static_cast<int>(params_.damage),
			params_.projectileLifeTime,
			origin,
			0u,
			static_cast<uint32_t>(CollisionTypeIdDef::kBullet),
			params_
		);
	}
}

float WeaponInstance::GetBaseSpreadDeg() const
{
	return st_.isADS ? params_.baseAdsSpreadDeg : params_.baseHipSpreadDeg;
}
