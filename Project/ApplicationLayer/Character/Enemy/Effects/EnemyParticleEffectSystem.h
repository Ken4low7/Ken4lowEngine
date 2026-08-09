#pragma once

#include "Vector3.h"

namespace K4E = ::Ken4lowEngine;

class EnemyParticleEffectSystem
{
public:
    void Initialize();
    bool IsInitialized() const { return isInitialized_; }

    // 敵生成地点へ煙と地面リングを出し、出現位置を事前に伝える。
    void SpawnAppearEffect(const K4E::Vector3& spawnWorldPos);

    // 被弾位置が取れている場合はこちら
    void SpawnHitEffect(const K4E::Vector3& hitWorldPos);

    // 死亡位置（敵中心足元あたりを想定）
    void SpawnDeathEffect(const K4E::Vector3& deathWorldPos);

private:
    bool isInitialized_ = false;

private:
    static K4E::Vector3 AddY(const K4E::Vector3& v, float y);
};
