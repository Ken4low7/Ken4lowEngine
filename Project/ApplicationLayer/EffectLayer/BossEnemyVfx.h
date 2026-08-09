#pragma once
#include "BaseGpuVfx.h"
#include "GpuParticleType.h"
#include "BillboardMode.h"

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
///          Boss専用VFX（登場・攻撃・死亡演出）
/// -------------------------------------------------------------
class BossEnemyVfx : public BaseGpuVfx
{
public: /// ---------- メンバ関数 ---------- ///

	// 仮想デストラクタ
	virtual ~BossEnemyVfx() = default;

	// 砂埃エフェクト更新
	void UpdateAppearDust(const K4E::Vector3& position, uint32_t count);

	// 登場時のMesh破片エフェクト更新
	void UpdateAppearMeshDebris(const K4E::Vector3& position, uint32_t count);

	// ボスオーラエフェクト更新
	void UpdateAura(const K4E::Vector3& position, uint32_t count);

	// ラッシュ軌跡エフェクト更新
	void UpdateRushTrail(float deltaTime, const K4E::Vector3& currentPosition, const K4E::Vector3& oldPosition, bool isRush);

	// ラッシュヒットエフェクト更新
	void UpdateRushHit(const K4E::Vector3& hitPosition, const K4E::Vector3& bossCenter);

	// スピン攻撃エフェクト更新
	void UpdateSpinAttack(const K4E::Vector3& currentPosition, bool isSpin);

	// 死亡時エフェクト更新
	void UpdateDeathEffect(const K4E::Vector3& center, float deathTimer, bool& startBurstDone);

	// 連続発生防止のクールダウン更新
	void Tick(float deltaTime) { if (rushHitCooldown_ > 0.0f) rushHitCooldown_ -= deltaTime; }

	// 念のため（再初期化などで使える）
	void Reset() override
	{
		appearDustEmitter_ = nullptr;
		appearMeshDebrisEmitter_ = nullptr;
		auraEmitter_ = nullptr;
		rushTrailEmitter_ = nullptr;
		rushHitEmitter_ = nullptr;
		spinAttackEmitter_ = nullptr;
		deathExplosionEmitter_ = nullptr;
		deathShockwaveEmitter_ = nullptr;
		deathSoulEmitter_ = nullptr;
		debrisDustEmitter_ = nullptr;
		rushHitCooldown_ = 0.0f;
	}

protected: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 派生先でエミッターを登録する純粋仮想メソッド。
	/// </summary>
	void RegisterEmitters() override;

private: /// ---------- メンバ変数 ---------- ///

	K4E::GpuParticleEmitter* appearDustEmitter_ = nullptr;       // 登場時の砂埃エミッター
	K4E::GpuParticleEmitter* appearMeshDebrisEmitter_ = nullptr; // 登場時のMesh破片エミッター
	K4E::GpuParticleEmitter* auraEmitter_ = nullptr;             // ボスオーラエミッター
	K4E::GpuParticleEmitter* rushTrailEmitter_ = nullptr;        // ラッシュ軌跡用エミッター
	K4E::GpuParticleEmitter* rushHitEmitter_ = nullptr;          // ラッシュヒット用エミッター
	K4E::GpuParticleEmitter* spinAttackEmitter_ = nullptr;       // スピン攻撃用エミッター
	K4E::GpuParticleEmitter* deathExplosionEmitter_ = nullptr;   // 中心爆発
	K4E::GpuParticleEmitter* deathShockwaveEmitter_ = nullptr;   // 足元衝撃波リング
	K4E::GpuParticleEmitter* deathSoulEmitter_ = nullptr;        // 魂が昇る
	K4E::GpuParticleEmitter* debrisDustEmitter_ = nullptr;       // 破片の埃（余韻）

	float rushHitCooldown_ = 0.0f; // 連続発生防止(秒)
};
