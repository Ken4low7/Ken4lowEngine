#include "AttackBehaviors.h"

#include "CharacterActor.h"
#include "CharacterMovementComponent.h"
#include "SceneComponent.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{
	namespace
	{
		constexpr float kChargeDriveForce = 5000.0f;
		constexpr float kChargeFacingSpeed = 1000.0f;

		/// 単一TargetへDamageを適用し、共通攻撃イベント用の結果へ変換する。
		AttackExecutionResult ApplyDamageOnce(AttackContext& context, float damage)
		{
			AttackExecutionResult execution{};
			execution.executed = true;
			if (!context.target || context.target->IsDead()) return execution;

			CharacterDamageInfo damageInfo{};
			damageInfo.amount = damage;
			damageInfo.sourceActor = context.owner;
			if (context.target)
			{
				damageInfo.hitPosition = context.target->GetTargetPosition();
				damageInfo.hasHitPosition = true; // 攻撃者と命中位置をDamageへ残し、被弾方向UIと将来の着弾演出で共有する。
			}
			const CharacterDamageResult damageResult = context.target->ApplyDamage(damageInfo);
			execution.accepted = damageResult.accepted;
			execution.appliedDamage = damageResult.appliedDamage;
			return execution;
		}

		/// 受理された近接Damageだけ、攻撃者から離れる方向へTarget Movementへノックバックを要求する。
		void ApplyAcceptedKnockback(const AttackContext& context, const AttackExecutionResult& execution, float horizontalPower, float verticalPower)
		{
			if (!execution.accepted || !context.owner || !context.target) return;
			CharacterMovementComponent* movement = context.target->GetMovementComponent();
			const SceneComponent* ownerRoot = context.owner->GetRootComponent();
			if (!movement || !ownerRoot) return;

			const Vector3 direction = context.target->GetTargetPosition() - ownerRoot->GetWorldPosition();
			movement->ApplyDamageKnockback(direction, horizontalPower, verticalPower); // PlayerMovement派生では入力より優先する減衰ノックバックへ接続される。
		}

		/// XZ平面上の方向を0除算せず正規化する。
		Vector3 NormalizeXZ(const Vector3& value)
		{
			const float length = Vector3::LengthXZ(value);
			return length > 0.0001f ? Vector3{ value.x / length, 0.0f, value.z / length } : Vector3{};
		}

		/// 攻撃Active中も水平距離とY差の両方を確認し、高所Targetへの不自然な命中を防ぐ。
		bool IsWithinAttackVolume(const AttackContext& context, const AttackData& data)
		{
			return context.distanceToTarget >= data.minRange &&
				context.distanceToTarget <= data.maxRange &&
				context.heightDifferenceToTarget <= data.maxHeightDifference;
		}
	}

	bool MeleeAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.target && !context.target->IsDead() && IsWithinAttackVolume(context, data);
	}

	void MeleeAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		executed_ = false; // 同じActive中に複数フレーム更新されても1回だけ命中させる。
	}

	AttackExecutionResult MeleeAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (executed_) return {};
		executed_ = true;
		if (!IsWithinAttackVolume(context, data)) return { true, false, 0.0f };
		const AttackExecutionResult execution = ApplyDamageOnce(context, data.damage);
		ApplyAcceptedKnockback(context, execution, 5.5f, 1.4f);
		return execution;
	}

	void MeleeAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)context;
		(void)data;
		(void)interrupted;
		executed_ = false;
	}

	bool ProjectileAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.target && !context.target->IsDead() && IsWithinAttackVolume(context, data);
	}

	void ProjectileAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		fired_ = false; // 発射物生成へ差し替える場合もこの一度きりの発射点を共有する。
	}

	AttackExecutionResult ProjectileAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (fired_) return {};
		fired_ = true;
		if (!IsWithinAttackVolume(context, data)) return { true, false, 0.0f };
		return ApplyDamageOnce(context, data.damage);
	}

	void ProjectileAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)context;
		(void)data;
		(void)interrupted;
		fired_ = false;
	}

	bool ChargeAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.owner && context.target && data.movementSpeed > 0.0f && IsWithinAttackVolume(context, data);
	}

	void ChargeAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)data;
		chargeDirection_ = {};
		directionLocked_ = false;
		hit_ = false;
		driveForceOverridden_ = false;
		if (!context.owner) return;
		if (CharacterMovementComponent* movement = context.owner->GetMovementComponent())
		{
			originalDriveForce_ = movement->GetMaxDriveForce();
			movement->SetMaxDriveForce(std::max(originalDriveForce_, kChargeDriveForce)); // 突進中だけ加速上限を上げ、重いBossでも設定速度へすぐ到達させる。
			driveForceOverridden_ = true;
		}
	}

	AttackExecutionResult ChargeAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)normalizedActiveTime;
		if (!context.owner || !context.target) return {};
		CharacterMovementComponent* movement = context.owner->GetMovementComponent();
		const SceneComponent* root = context.owner->GetRootComponent();
		if (movement && root)
		{
			if (!directionLocked_)
			{
				chargeDirection_ = NormalizeXZ(context.target->GetTargetPosition() - root->GetWorldPosition());
				directionLocked_ = Vector3::LengthXZ(chargeDirection_) > 0.0001f; // Active開始時の方向だけを保存し、突進中のホーミングを止める。
			}
			if (directionLocked_)
			{
				movement->FaceDirectionXZ(chargeDirection_, kChargeFacingSpeed, deltaTime);
				movement->SetVelocity(chargeDirection_ * data.movementSpeed);
			}
		}

		const float contactRange = std::max(1.2f, data.minRange + 0.35f);
		const bool reachedTarget = context.distanceToTarget <= contactRange &&
			context.heightDifferenceToTarget <= data.maxHeightDifference;
		if (hit_ || !reachedTarget) return {}; // 発動可能な遠距離ではなく、実際にTargetへ接触した距離だけDamageを通す。

		hit_ = true;
		const AttackExecutionResult execution = ApplyDamageOnce(context, data.damage);
		ApplyAcceptedKnockback(context, execution, 8.0f, 2.2f);
		return execution;
	}

	void ChargeAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)data;
		(void)interrupted;
		if (context.owner)
		{
			if (CharacterMovementComponent* movement = context.owner->GetMovementComponent())
			{
				movement->Stop();
				if (driveForceOverridden_) movement->SetMaxDriveForce(originalDriveForce_);
			}
		}
		chargeDirection_ = {};
		directionLocked_ = false;
		driveForceOverridden_ = false;
		hit_ = false;
	}

	bool ShockwaveAttackBehavior::CanStart(const AttackContext& context, const AttackData& data) const
	{
		return context.target && IsWithinAttackVolume(context, data);
	}

	void ShockwaveAttackBehavior::Begin(AttackContext& context, const AttackData& data)
	{
		(void)context;
		(void)data;
		emitted_ = false; // 将来の範囲検索へ差し替えても衝撃波生成を1回に保つ。
	}

	AttackExecutionResult ShockwaveAttackBehavior::Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime)
	{
		(void)deltaTime;
		(void)normalizedActiveTime;
		if (emitted_) return {};
		emitted_ = true;
		if (!IsWithinAttackVolume(context, data)) return { true, false, 0.0f };
		return ApplyDamageOnce(context, data.damage);
	}

	void ShockwaveAttackBehavior::End(AttackContext& context, const AttackData& data, bool interrupted)
	{
		(void)context;
		(void)data;
		(void)interrupted;
		emitted_ = false;
	}

	std::unique_ptr<IAttackBehavior> CreateAttackBehavior(std::string_view behaviorType)
	{
		using Factory = std::function<std::unique_ptr<IAttackBehavior>()>;
		static const std::unordered_map<std::string, Factory> factories = {
			{ "Melee", [] { return std::make_unique<MeleeAttackBehavior>(); } },
			{ "Projectile", [] { return std::make_unique<ProjectileAttackBehavior>(); } },
			{ "Charge", [] { return std::make_unique<ChargeAttackBehavior>(); } },
			{ "Shockwave", [] { return std::make_unique<ShockwaveAttackBehavior>(); } }
		}; // 個別クラスをFactory登録し、攻撃種別追加で巨大なswitchを増やさない。
		const auto factoryIt = factories.find(std::string(behaviorType));
		return factoryIt != factories.end() ? factoryIt->second() : nullptr;
	}
} // namespace Ken4lowEngine
