#pragma once
#include "PhysicsTypes.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         剛体クラス
	/// -------------------------------------------------------------
	class Rigidbody
	{
	public: /// ---------- メンバ関数 ---------- ///

		// BodyTypeを設定し、Staticの場合は質量計算から外す。
		void SetBodyType(BodyType bodyType);
		BodyType GetBodyType() const { return bodyType_; }

		// 質量を設定し、逆質量を同時に更新する。
		void SetMass(float mass);
		float GetMass() const { return mass_; }
		float GetInvMass() const { return invMass_; }

		// 形状ごとの単位質量あたり慣性モーメントを設定する。
		void SetInertiaScale(const Vector3& inertiaScale);
		Vector3 GetInertiaScale() const { return inertiaScale_; }
		Vector3 GetInvInertia() const { return invInertia_; }

		// 重力適用フラグを設定する。
		void SetUseGravity(bool useGravity) { useGravity_ = useGravity; }
		bool IsUseGravity() const { return useGravity_; }

		// PhysicsWorldから反映される重力加速度を設定する。
		void SetGravity(const Vector3& gravity) { gravity_ = gravity; }
		Vector3 GetGravity() const { return gravity_; }

		// 反発係数を設定する。0.0で跳ねず、1.0に近いほど反発を強くする。
		void SetRestitution(float restitution);
		float GetRestitution() const { return restitution_; }

		// 静止摩擦係数を設定する。現段階では値保持を行い、将来の応答拡張に備える。
		void SetStaticFriction(float staticFriction);
		float GetStaticFriction() const { return staticFriction_; }

		// 動摩擦係数を設定する。接触面に沿った速度減衰に使用する。
		void SetDynamicFriction(float dynamicFriction);
		float GetDynamicFriction() const { return dynamicFriction_; }

		// 接地状態を設定する。PhysicsWorldのContact解決時に毎フレーム再計算される。
		void SetGrounded(bool isGrounded) { isGrounded_ = isGrounded; }
		bool IsGrounded() const { return isGrounded_; }

		// Sleep機能の有効状態を設定する。
		void SetSleepEnabled(bool enabled);
		bool IsSleepEnabled() const { return sleepEnabled_; }

		// Sleep状態を設定する。
		void SetSleeping(bool isSleeping);
		bool IsSleeping() const { return isSleeping_; }

		// 外力や速度変更に備えてSleep状態から復帰する。
		void WakeUp();

		// 停止状態が続いた場合にSleep状態へ移行する。
		void UpdateSleepState(float deltaTime);

		// Sleep判定用の速度閾値を設定する。
		void SetSleepSpeedThreshold(float threshold);
		float GetSleepSpeedThreshold() const { return sleepSpeedThreshold_; }

		// Sleep判定用の継続時間閾値を設定する。
		void SetSleepTimeThreshold(float threshold);
		float GetSleepTimeThreshold() const { return sleepTimeThreshold_; }
		float GetSleepTimer() const { return sleepTimer_; }

		// Contactから再計算されるフレーム状態をクリアする。
		void ClearFrameState();

		// 力を蓄積し、次のIntegrateで速度へ反映する。
		void AddForce(const Vector3& force);

		// ワールド座標上の点へ力を加え、並進力とr×FのTorqueへ分解する。
		void AddForceAtPosition(const Vector3& force, const Vector3& worldPosition, const Vector3& centerOfMass);

		// Torqueを蓄積し、次のIntegrateで角速度へ反映する。
		void AddTorque(const Vector3& torque);

		// 蓄積された力とTorqueをクリアする。
		void ClearForces();
		void ClearTorques();

		// 速度を直接設定する。
		void SetVelocity(const Vector3& velocity);

		// 現在の速度を取得する。
		Vector3 GetVelocity() const;

		// 角速度を直接設定する。
		void SetAngularVelocity(const Vector3& angularVelocity);

		// 現在の角速度を取得する。
		Vector3 GetAngularVelocity() const;

		// 蓄積された力/Torqueを速度/角速度へ積分し、Accumulatorをクリアする。
		void Integrate(float deltaTime);

	private: /// ---------- 内部処理 ---------- ///

		// BodyType・Mass・形状慣性から逆質量と逆慣性を更新する。
		void UpdateMassProperties();

	private: /// ---------- メンバ変数 ---------- ///

		// 剛体の動作種別。
		BodyType bodyType_ = BodyType::Dynamic;

		// 現在の並進速度と角速度。
		Vector3 velocity_{};
		Vector3 angularVelocity_{};

		// このステップで蓄積された力とTorque。
		Vector3 force_{};
		Vector3 torque_{};

		// 質量と逆質量。
		float mass_ = 1.0f;
		float invMass_ = 1.0f;

		// 単位質量あたりの主軸慣性と、その逆慣性。
		Vector3 inertiaScale_{ 1.0f, 1.0f, 1.0f };
		Vector3 invInertia_{ 1.0f, 1.0f, 1.0f };

		// 接触時の反発係数。
		float restitution_ = 0.0f;

		// 摩擦係数。
		float staticFriction_ = 0.5f;
		float dynamicFriction_ = 0.2f;

		// 重力を速度へ反映するか。
		bool useGravity_ = false;

		// 重力加速度。既定値は既存挙動と同じ-9.8m/s^2相当。
		Vector3 gravity_{ 0.0f, -9.8f, 0.0f };

		// このフレームで床に接触しているか。
		bool isGrounded_ = false;

		// Sleep状態と判定用タイマー。
		bool sleepEnabled_ = true;
		bool isSleeping_ = false;
		float sleepTimer_ = 0.0f;
		float sleepSpeedThreshold_ = 0.05f;
		float sleepTimeThreshold_ = 0.5f;
	};

} // namespace Ken4lowEngine
