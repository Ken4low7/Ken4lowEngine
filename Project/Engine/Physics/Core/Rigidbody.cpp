#include "Rigidbody.h"

#include <algorithm>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kMinimumMass = 0.0001f;
	}

	void Rigidbody::SetBodyType(BodyType bodyType)
	{
		bodyType_ = bodyType;

		// Static/Kinematicは物理積分で動かさないため、逆質量を0として扱う。
		if (bodyType_ == BodyType::Static || bodyType_ == BodyType::Kinematic)
		{
			invMass_ = 0.0f;
		}
		else
		{
			SetMass(mass_);
		}
	}

	void Rigidbody::SetMass(float mass)
	{
		mass_ = std::max(mass, kMinimumMass);

		// Static/Kinematicは質量値を保持しつつ、応答計算では無限質量として扱う。
		invMass_ = (bodyType_ == BodyType::Static || bodyType_ == BodyType::Kinematic) ? 0.0f : (1.0f / mass_);
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
		// Sleepへ入るときは速度と力を消し、次フレームへ微小な残りを持ち越さない。
		isSleeping_ = sleepEnabled_ && isSleeping;
		if (isSleeping_)
		{
			velocity_ = {};
			force_ = {};
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

		const float speedSquared = Vector3::LengthSquared(velocity_);
		const float thresholdSquared = sleepSpeedThreshold_ * sleepSpeedThreshold_;
		if (speedSquared <= thresholdSquared)
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

	void Rigidbody::ClearForces()
	{
		// Resetや外部制御から次ステップへ持ち越したくない力を破棄する。
		force_ = {};
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

	void Rigidbody::Integrate(float deltaTime)
	{
		// 不正な時間、Dynamic以外、Sleep中のBodyは速度積分を行わない。
		if (deltaTime <= 0.0f || bodyType_ != BodyType::Dynamic || isSleeping_)
		{
			force_ = {};
			return;
		}

		Vector3 acceleration = force_ * invMass_;
		if (useGravity_)
		{
			acceleration += gravity_;
		}

		// 現段階では位置更新先を持たず、速度だけを更新して将来のTransform接続に備える。
		velocity_ += acceleration * deltaTime;
		force_ = {};
	}

} // namespace Ken4lowEngine
