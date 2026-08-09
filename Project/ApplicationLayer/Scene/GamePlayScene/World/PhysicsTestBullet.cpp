#include "PhysicsTestBullet.h"

#include "Object3D.h"

void PhysicsTestBullet::Initialize(uint32_t collisionLayer)
{
	// TriggerEvent確認用の弾はSolver対象にせず、Kinematic ColliderとしてPhysicsWorldへ登録する。
	rigidbody_.SetBodyType(K4E::BodyType::Kinematic);
	rigidbody_.SetUseGravity(false);
	rigidbody_.SetSleepEnabled(false);
	rigidbody_.SetVelocity({});

	collider_.SetRigidbody(&rigidbody_);
	collider_.SetCollisionLayer(collisionLayer);
	collider_.SetTrigger(true);
	collider_.SetOwner<PhysicsTestBullet>(this);
	collider_.SetEnabled(false);

	object3d_ = std::make_unique<K4E::Object3D>();
	object3d_->Initialize("Sample/cube.gltf");
	object3d_->SetScale(halfSize_ * 2.0f);
	object3d_->SetColor({ 1.0f, 0.9f, 0.15f, 1.0f });
	object3d_->Update();
}

void PhysicsTestBullet::Update(float deltaTime)
{
	if (!isAlive_)
	{
		return;
	}

	// Kinematic弾の移動結果だけをPhysicsWorldのTrigger判定へ渡す。
	position_ += velocity_ * deltaTime;
	rigidbody_.SetVelocity(velocity_);
	SyncCollider();

	if (object3d_)
	{
		object3d_->SetTranslate(position_);
		object3d_->Update();
	}
}

void PhysicsTestBullet::Draw()
{
	if (isAlive_ && object3d_)
	{
		object3d_->Draw();
	}
}

void PhysicsTestBullet::Reset(const K4E::Vector3& position, const K4E::Vector3& velocity)
{
	// 発射状態を初期化し、TriggerEnterが再度発生するようColliderを有効化する。
	position_ = position;
	velocity_ = velocity;
	isAlive_ = true;
	collider_.SetEnabled(true);
	rigidbody_.SetVelocity(velocity_);
	rigidbody_.ClearForces();
	rigidbody_.ClearFrameState();
	rigidbody_.WakeUp();
	SyncCollider();

	if (object3d_)
	{
		object3d_->SetTranslate(position_);
		object3d_->Update();
	}
}

void PhysicsTestBullet::Kill()
{
	// 既存Bullet処理へ影響させず、テスト弾だけをTrigger判定から外す。
	isAlive_ = false;
	velocity_ = {};
	rigidbody_.SetVelocity({});
	collider_.SetEnabled(false);
}

void PhysicsTestBullet::SyncCollider()
{
	// 表示位置とPhysicsWorldで検出するAABBを一致させる。
	collider_.SetAABB({
		position_ - halfSize_,
		position_ + halfSize_,
		});
}
