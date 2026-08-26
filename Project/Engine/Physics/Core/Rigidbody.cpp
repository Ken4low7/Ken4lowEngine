#include "Rigidbody.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kMinimumMass = 0.0001f;
		constexpr float kMinimumInertiaScale = 0.0001f;
	}

	void Rigidbody::SetBodyType(BodyType bodyType)
	{
		bodyType_ = bodyType;
		UpdateMassProperties();

		if (bodyType_ == BodyType::Static || bodyType_ == BodyType::Kinematic)
		{
			velocity_ = {};
			angularVelocity_ = {};
			force_ = {};
			torque_ = {};
		}
	}

	void Rigidbody::SetMass(float mass)
	{
		mass_ = std::max(mass, kMinimumMass);
		UpdateMassProperties();
	}

	void Rigidbody::SetInertiaScale(const Vector3& inertiaScale)
	{
		inertiaScale_ = {
			std::max(inertiaScale.x, kMinimumInertiaScale),
			std::max(inertiaScale.y, kMinimumInertiaScale),
			std::max(inertiaScale.z, kMinimumInertiaScale)
		};
		UpdateMassProperties();
	}

	void Rigidbody::SetRestitution(float restitution)
	{
		// 反発係数は速度補正が暴れないよう、一般的な0.0〜1.0に丸める。
		restitution_ = std::clamp(restitution, 0.0f, 1.0f);
	}

	void Rigidbody::SetStaticFriction(float staticFriction)
	{
		// 摩擦係数は負値を許容せず、将来の静止摩擦応答でそのまま使える値にする。
		staticFriction_ = std::max(staticFriction, 0.0f);
	}

	void Rigidbody::SetDynamicFriction(float dynamicFriction)
	{
		// 動摩擦係数は負値を許容せず、接触面方向の減速量として扱う。
		dynamicFriction_ = std::max(dynamicFriction, 0.0f);
	}

	void Rigidbody::SetSleepEnabled(bool enabled)
	{
		// Sleep無効時は即座に起こし、以降のSleep判定も止める。
		sleepEnabled_ = enabled;
		if (!sleepEnabled_)
		{
			WakeUp();
		}
	}

	void Rigidbody::SetSleeping(bool isSleeping)
	{
		// Sleepへ入るときは並進/回転の速度とAccumulatorを消し、次フレームへ残さない。
		isSleeping_ = sleepEnabled_ && isSleeping;
		if (isSleeping_)
		{
			velocity_ = {};
			angularVelocity_ = {};
			force_ = {};
			torque_ = {};
			sleepTimer_ = sleepTimeThreshold_;
		}
		else
		{
			sleepTimer_ = 0.0f;
		}
	}

	void Rigidbody::WakeUp()
	{
		// 外部入力や衝突応答で再び動かせるようにSleep状態を解除する。
		isSleeping_ = false;
		sleepTimer_ = 0.0f;
	}

	void Rigidbody::UpdateSleepState(float deltaTime)
	{
		// 停止状態が続いた物体をSleepへ移行する。Dynamic以外やSleep無効時は常に起きた状態にする。
		if (!sleepEnabled_ || bodyType_ != BodyType::Dynamic)
		{
			WakeUp();
			return;
		}
		if (isSleeping_)
		{
			return;
		}

		const float linearSpeedSquared = Vector3::LengthSquared(velocity_);
		const float angularSpeedSquared = Vector3::LengthSquared(angularVelocity_);
		const float thresholdSquared = sleepSpeedThreshold_ * sleepSpeedThreshold_;
		if (linearSpeedSquared <= thresholdSquared && angularSpeedSquared <= thresholdSquared)
		{
			sleepTimer_ += std::max(deltaTime, 0.0f);
			if (sleepTimer_ >= sleepTimeThreshold_)
			{
				SetSleeping(true);
			}
			return;
		}

		sleepTimer_ = 0.0f;
	}

	void Rigidbody::SetSleepSpeedThreshold(float threshold)
	{
		// Sleep判定の速度閾値は負値にせず、UIからの調整を安全に受ける。
		sleepSpeedThreshold_ = std::max(threshold, 0.0f);
	}

	void Rigidbody::SetSleepTimeThreshold(float threshold)
	{
		// Sleep判定の時間閾値は負値にせず、即Sleepしたい場合は0.0を許容する。
		sleepTimeThreshold_ = std::max(threshold, 0.0f);
	}

	void Rigidbody::ClearFrameState()
	{
		// 接地などの接触由来の状態は、PhysicsWorldのContactから毎フレーム作り直す。
		isGrounded_ = false;
	}

	void Rigidbody::AddForce(const Vector3& force)
	{
		// Dynamic以外は外力で速度を変えない。
		if (bodyType_ != BodyType::Dynamic)
		{
			return;
		}

		force_ += force;
		if (Vector3::LengthSquared(force) > 0.0f)
		{
			WakeUp();
		}
	}

	void Rigidbody::AddForceAtPosition(const Vector3& force, const Vector3& worldPosition, const Vector3& centerOfMass)
	{
		if (bodyType_ != BodyType::Dynamic)
		{
			return;
		}

		AddForce(force);
		AddTorque(Vector3::Cross(worldPosition - centerOfMass, force)); // 偏った浮力をr×Fの回転力へ変換する。
	}

	void Rigidbody::AddTorque(const Vector3& torque)
	{
		if (bodyType_ != BodyType::Dynamic)
		{
			return;
		}

		torque_ += torque;
		if (Vector3::LengthSquared(torque) > 0.0f)
		{
			WakeUp();
		}
	}

	void Rigidbody::ClearForces()
	{
		// Resetや外部制御から次ステップへ持ち越したくない力を破棄する。
		force_ = {};
	}

	void Rigidbody::ClearTorques()
	{
		// TorqueもForceと同様に1ステップだけ有効なAccumulatorとして扱う。
		torque_ = {};
	}

	void Rigidbody::SetVelocity(const Vector3& velocity)
	{
		// Static/Kinematicは速度を持たせず、将来の応答対象からも外しやすくする。
		velocity_ = (bodyType_ == BodyType::Static || bodyType_ == BodyType::Kinematic) ? Vector3{} : velocity;
		if (Vector3::LengthSquared(velocity_) > 0.0f)
		{
			WakeUp();
		}
	}

	Vector3 Rigidbody::GetVelocity() const
	{
		// 速度は読み取り専用で返し、外部からの変更はSetVelocityへ集約する。
		return velocity_;
	}

	void Rigidbody::SetAngularVelocity(const Vector3& angularVelocity)
	{
		angularVelocity_ = (bodyType_ == BodyType::Static || bodyType_ == BodyType::Kinematic) ? Vector3{} : angularVelocity;
		if (Vector3::LengthSquared(angularVelocity_) > 0.0f)
		{
			WakeUp();
		}
	}

	Vector3 Rigidbody::GetAngularVelocity() const
	{
		return angularVelocity_;
	}

	void Rigidbody::Integrate(float deltaTime)
	{
		// 不正な時間、Dynamic以外、Sleep中のBodyは速度積分を行わない。
		if (deltaTime <= 0.0f || bodyType_ != BodyType::Dynamic || isSleeping_)
		{
			force_ = {};
			torque_ = {};
			return;
		}

		Vector3 acceleration = force_ * invMass_;
		if (useGravity_)
		{
			acceleration += gravity_;
		}

		const Vector3 angularAcceleration{
			torque_.x * invInertia_.x,
			torque_.y * invInertia_.y,
			torque_.z * invInertia_.z
		};

		velocity_ += acceleration * deltaTime;
		angularVelocity_ += angularAcceleration * deltaTime;
		force_ = {};
		torque_ = {};
	}

	void Rigidbody::UpdateMassProperties()
	{
		if (bodyType_ == BodyType::Static || bodyType_ == BodyType::Kinematic)
		{
			invMass_ = 0.0f;
			invInertia_ = {};
			return;
		}

		invMass_ = 1.0f / std::max(mass_, kMinimumMass);
		invInertia_ = {
			1.0f / std::max(mass_ * inertiaScale_.x, kMinimumMass * kMinimumInertiaScale),
			1.0f / std::max(mass_ * inertiaScale_.y, kMinimumMass * kMinimumInertiaScale),
			1.0f / std::max(mass_ * inertiaScale_.z, kMinimumMass * kMinimumInertiaScale)
		};
	}

} // namespace Ken4lowEngine
