#include "ParticleEmitter.h"
#include <LogString.h>
#include <DirectXCommon.h>
#include <ParticleManager.h>
#include <GameTimer.h>

namespace K4E = ::Ken4lowEngine;


/// -------------------------------------------------------------
///				　		コンストラクタ
/// -------------------------------------------------------------
ParticleEmitter::ParticleEmitter(K4E::ParticleManager* manager, const std::string& groupName)
    : particleManager_(manager), groupName_(groupName), position_({ 0.0f,0.0f,0.0f }),
    emissionRate_(10.0f), accumulatedTime_(0.0f)
{
}


/// -------------------------------------------------------------
///				　		更新処理
/// -------------------------------------------------------------
void ParticleEmitter::Update()
{
	accumulatedTime_ += K4E::GameTimer::GetInstance()->GetDeltaTime();

    int particleCount = static_cast<int>(emissionRate_);
    if (particleCount > 0)
    {
        // グループに設定された type を取得
        K4E::ParticleEffectType type = particleManager_->GetGroupType(groupName_);

        // 自動で対応した type を使って射出
        particleManager_->Emit(groupName_, position_, particleCount, type);

		// 発生済み分を減算
        accumulatedTime_ -= static_cast<float>(particleCount) / emissionRate_;
    }
}


/// -------------------------------------------------------------
///				　		バースト射出
/// -------------------------------------------------------------
void ParticleEmitter::Burst(int count)
{
    K4E::ParticleEffectType type = particleManager_->GetGroupType(groupName_);
    particleManager_->Emit(groupName_, position_, count, type);
}