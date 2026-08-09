#pragma once

#include <cstdint>
#include "WeaponParams.h"

class BulletManager;
class CollisionManager;

namespace Ken4lowEngine { class Camera; }
namespace K4E = ::Ken4lowEngine;

/// <summary>
/// 武器ごとの実行時状態を保持する
/// マスターデータ由来の固定値とは分け、残弾・クールダウン・リロード・拡散などゲーム中に変化する値だけを扱う
/// </summary>
struct WeaponRuntimeState
{
	int32_t magAmmo = 0;
	int32_t reserveAmmo = 0;

	float fireCooldown = 0.0f;

	bool isReloading = false;
	bool reloadRequested = false;
	bool reloadRequest = false;
	bool pendingReload = false;
	float reloadTimer = 0.0f;

	int32_t burstRemaining = 0;
	float   burstTimer = 0.0f;

	// 射撃ごとに増える動的拡散。Hip/ADSの基礎拡散に加算して使用する
	float spread = 0.0f;

	// ADS状態はPlayer側の入力結果を毎フレーム反映し、拡散やUI表示に使用する
	bool isADS = false;
};

/// <summary>
/// 装備中武器1本分のランタイム処理を担当する。
/// Playerから射撃入力とADS状態を受け取り、弾生成・クールダウン・リロード・バースト状態を更新する
/// </summary>
class WeaponInstance
{
public:
	void Equip(const WeaponParams& p);

	void Tick(float dt);

	void StartReload();

	/// <summary>
	/// 入力状態に応じて射撃可能か判定し、可能であれば弾生成と弾薬・クールダウン更新を行う
	/// </summary>
	void TryFire(bool fireHeld, bool firePressed,
		K4E::Camera* cam,
		BulletManager* bulletMgr,
		CollisionManager* colMgr);

	// 現在の発射モードをUIや入力処理へ返す
	bool IsAutomatic() const { return fireModeAutomatic_; }
	bool CanToggleFireMode() const { return params_.canToggleFireMode; }
	bool ToggleFireMode();

	// Player側で決定したADS状態を武器側の拡散計算へ反映する
	void SetADS(bool v) { st_.isADS = v; }

	// 現在のマガジン状態に応じて、通常/タクティカル/空リロードの時間を返す
	float GetCurrentReloadDurationSec() const;

	const WeaponParams& Params() const { return params_; }
	const WeaponRuntimeState& State() const { return st_; }
	WeaponRuntimeState& StateMutable() { return st_; }

	int32_t AddReserveAmmo(int32_t amount);
	int32_t GetMagazineAmmo() const { return st_.magAmmo; }
	int32_t GetReserveAmmo() const { return st_.reserveAmmo; }
	int32_t GetMaxReserveAmmo() const { return params_.maxReserveAmmo; }

	// 中断可能な武器だけ、リロード状態と予約状態を解除する
	void CancelReload();

private: /// ---------- メンバ関数 ---------- ///

	bool CanStartReload() const;
	void StartReloadInternal();

	bool WantFire(bool fireHeld, bool firePressed) const;
	bool CanFire() const;

	void ConsumeAmmo();
	void FinishReload();

	void FireShot(K4E::Camera* cam, BulletManager* bulletMgr, CollisionManager* colMgr);

	// ADS状態に応じて、腰だめ/ADSどちらの基礎拡散を使うか決める
	float GetBaseSpreadDeg() const;

private:
	WeaponParams params_{};
	WeaponRuntimeState st_{};

	bool fireModeAutomatic_ = false; // trueなら押しっぱなし射撃、falseなら単発入力射撃として扱う
};
