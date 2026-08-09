#pragma once

#include "Engine/Physics/Collision/Core/Collider.h"
#include "Engine/Physics/Core/Rigidbody.h"
#include "Vector3.h"

#include <memory>

namespace Ken4lowEngine
{
	class Object3D;
}

namespace K4E = ::Ken4lowEngine;

/// -------------------------------------------------------------
/// PhysicsWorldのTriggerEvent確認用のテスト弾
/// -------------------------------------------------------------
class PhysicsTestBullet
{
public:
	// TriggerEvent確認用のCollider/Rigidbody/表示モデルを初期化する。
	void Initialize(uint32_t collisionLayer);

	// 現在速度で弾を移動し、PhysicsWorldへ渡すCollider位置を更新する。
	void Update(float deltaTime);

	// 生存中のテスト弾モデルを描画する。
	void Draw();

	// 指定位置と速度でテスト弾を再発射できる状態へ戻す。
	void Reset(const K4E::Vector3& position, const K4E::Vector3& velocity);

	// TriggerEvent判定に使うColliderを取得する。
	K4E::Collider* GetCollider() { return &collider_; }
	const K4E::Collider* GetCollider() const { return &collider_; }

	// PhysicsWorldへ登録するRigidbodyを取得する。
	K4E::Rigidbody* GetRigidbody() { return &rigidbody_; }
	const K4E::Rigidbody* GetRigidbody() const { return &rigidbody_; }

	// TriggerEnter確認後に弾を無効化する。
	void Kill();

	bool IsAlive() const { return isAlive_; }
	const K4E::Vector3& GetPosition() const { return position_; }
	const K4E::Vector3& GetVelocity() const { return velocity_; }
	const K4E::Vector3& GetHalfSize() const { return halfSize_; }

private:
	// 表示位置とCollider AABBを同期する。
	void SyncCollider();

private:
	std::unique_ptr<K4E::Object3D> object3d_;
	K4E::Rigidbody rigidbody_{};
	K4E::Collider collider_{};
	K4E::Vector3 position_{};
	K4E::Vector3 velocity_{};
	K4E::Vector3 halfSize_{ 0.25f, 0.25f, 0.25f };
	bool isAlive_ = false;
};
