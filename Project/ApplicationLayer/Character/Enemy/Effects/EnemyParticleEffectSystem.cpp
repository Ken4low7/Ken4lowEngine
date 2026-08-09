#include "EnemyParticleEffectSystem.h"

#include "GpuParticleManager.h"

using namespace Ken4lowEngine;

Vector3 EnemyParticleEffectSystem::AddY(const Vector3& v, float y)
{
    Vector3 out = v;
    out.y += y;
    return out;
}

void EnemyParticleEffectSystem::Initialize()
{
    if (isInitialized_)
    {
        return;
    }

    static bool enemyEffectsTuned = false;
    if (!enemyEffectsTuned)
    {
        auto* effects = EffectSystem::GetInstance();
        effects->RegisterSpriteEffect("EnemySpawnMist", GpuParticleType::Ambient, 44, 0, 0.0f, 0.42f);
        effects->RegisterSpriteEffect("EnemySpawnRing", GpuParticleType::Shockwave, 28, 0, 0.0f, 0.22f);
        effects->RegisterSpriteEffect("EnemyBlood", GpuParticleType::Blood, 32, 0, 0.0f, 0.18f);
        effects->RegisterSpriteEffect("EnemyDeathBlood", GpuParticleType::Blood, 56, 0, 0.0f, 0.28f);
        enemyEffectsTuned = true; // 生成・被弾・死亡EffectのRuntime登録を全Enemyで一度だけ共有する。
    }

    // Sprite系GPUパーティクルはEffectSystem側に登録済みのRuntime effectNameで再生する。
    // Enemy側ではEmitter名・ParameterManager登録・Json保存対象を直接管理しない。
    isInitialized_ = true;
}

void EnemyParticleEffectSystem::SpawnAppearEffect(const Vector3& spawnWorldPos)
{
    if (!isInitialized_)
    {
        return;
    }

    const Vector3 centerPos = spawnWorldPos;
    const Vector3 groundPos = AddY(spawnWorldPos, -1.85f);
    auto* effects = EffectSystem::GetInstance();
    effects->Play("EnemySpawnMist", centerPos);
    effects->Play("EnemySpawnRing", groundPos); // Enemy Rootは体の中心なので、リングだけCollider半高さ分下げて足元へ置く。
}

void EnemyParticleEffectSystem::SpawnHitEffect(const Vector3& hitWorldPos)
{
    // 未初期化のエフェクトシステムからプール番号を触らないようにする。
    if (!isInitialized_)
    {
        return;
    }

    auto* effects = EffectSystem::GetInstance();
    effects->Play("EnemyHitSpark", hitWorldPos);
    effects->Play("EnemyBlood", hitWorldPos); // 火花と血飛沫は同じ実命中座標から発生させる。
}

void EnemyParticleEffectSystem::SpawnDeathEffect(const Vector3& deathWorldPos)
{
    // 初期化前や終了後相当の状態では共有エミッターへアクセスしない。
    if (!isInitialized_)
    {
        return;
    }

    const Vector3 centerPos = AddY(deathWorldPos, 0.8f);
    const Vector3 chestPos = AddY(deathWorldPos, 1.0f);
    const Vector3 groundPos = AddY(deathWorldPos, 0.1f);

    auto* effects = EffectSystem::GetInstance();
    effects->Play("EnemyDeathSmoke", centerPos);
    effects->Play("EnemyDeathBlood", chestPos);
    effects->Play("EnemyDeathShock", groundPos);
}
