#pragma once

#include "Engine/Physics/Event/IPhysicsEventListener.h"

#include <string>

class PhysicsTestBullet;
class Bullet;

namespace Ken4lowEngine
{
	class Collider;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// Gameplay側でPhysicsEventを受け取り、テスト用ゲーム処理へ変換するクラス
/// -------------------------------------------------------------
class GameplayPhysicsEventHandler : public K4E::IPhysicsEventListener
{
public:
	// テスト弾とテストターゲットのColliderを登録し、判定対象を限定する。
	void Configure(PhysicsTestBullet* bullet, K4E::Collider* targetCollider);

	// TriggerEnterをゲーム側の処理へ変換する。
	void OnPhysicsEvent(const K4E::PhysicsEvent& event) override;

	// カウントと最新ログを初期化する。
	void Reset();

	int GetTriggerEnterCount() const { return triggerEnterCount_; }
	const std::string& GetLatestTriggerEvent() const { return latestTriggerEvent_; }
	bool HasTriggerHit() const { return hasTriggerHit_; }
	int GetRealBulletTriggerHitCount() const { return realBulletTriggerHitCount_; }
	const std::string& GetLatestRealBulletTriggerHit() const { return latestRealBulletTriggerHit_; }

private:
	// PhysicsEventがテスト弾とテストターゲットの組み合わせか判定する。
	bool IsBulletTargetPair(const K4E::PhysicsEvent& event) const;
	// TriggerEnterを実Bulletのヒット処理へ変換する。
	bool TryHandleBulletHit(const K4E::PhysicsEvent& event);
	Bullet* FindPhysicsBullet(const K4E::PhysicsEvent& event, K4E::Collider*& outOther) const;

private:
	PhysicsTestBullet* bullet_ = nullptr;
	K4E::Collider* targetCollider_ = nullptr;
	int triggerEnterCount_ = 0;
	int realBulletTriggerHitCount_ = 0;
	bool hasTriggerHit_ = false;
	std::string latestTriggerEvent_ = "None";
	std::string latestRealBulletTriggerHit_ = "None";
};
