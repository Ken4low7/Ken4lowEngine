#pragma once

#include "AttackComponent.h"
#include "Vector3.h"

#include <memory>
#include <string_view>

namespace Ken4lowEngine
{
	/// 近接攻撃のActive開始時に範囲内Targetへ一度だけダメージを適用する。
	class MeleeAttackBehavior final : public IAttackBehavior
	{
	public:
		/// Factory登録名として近接攻撃種別を返す。
		std::string_view GetTypeName() const override { return "Melee"; }
		/// Targetが生存し近接距離内にいるか判定する。
		bool CanStart(const AttackContext& context, const AttackData& data) const override;
		/// 攻撃ごとの一度きり命中フラグを戻す。
		void Begin(AttackContext& context, const AttackData& data) override;
		/// Active開始後の最初の更新だけ近接Damageを適用する。
		AttackExecutionResult Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime) override;
		/// 終了時に個別の実行フラグを破棄する。
		void End(AttackContext& context, const AttackData& data, bool interrupted) override;

	private:
		bool executed_ = false;
	};

	/// 射撃攻撃の発射タイミングを個別処理として管理し、Targetへ一度だけ命中を通知する。
	class ProjectileAttackBehavior final : public IAttackBehavior
	{
	public:
		/// Factory登録名として射撃攻撃種別を返す。
		std::string_view GetTypeName() const override { return "Projectile"; }
		/// Targetが生存し最小射程外にいるか判定する。
		bool CanStart(const AttackContext& context, const AttackData& data) const override;
		/// 攻撃ごとの一度きり発射フラグを戻す。
		void Begin(AttackContext& context, const AttackData& data) override;
		/// Active開始後の最初の更新だけ射撃命中を発生させる。
		AttackExecutionResult Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime) override;
		/// 終了時に個別の発射フラグを破棄する。
		void End(AttackContext& context, const AttackData& data, bool interrupted) override;

	private:
		bool fired_ = false;
	};

	/// 突進中だけOwnerへ固定方向の速度を与え、接近時に一度だけダメージを適用する。
	class ChargeAttackBehavior final : public IAttackBehavior
	{
	public:
		/// Factory登録名として突進攻撃種別を返す。
		std::string_view GetTypeName() const override { return "Charge"; }
		/// Owner、Target、突進速度が揃っているか判定する。
		bool CanStart(const AttackContext& context, const AttackData& data) const override;
		/// 攻撃ごとの一度きり接触フラグを戻す。
		void Begin(AttackContext& context, const AttackData& data) override;
		/// Active開始時に固定した方向の速度と接近時のDamageを共通Componentへ渡す。
		AttackExecutionResult Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime) override;
		/// 終了時に突進速度、固定方向、接触フラグを戻す。
		void End(AttackContext& context, const AttackData& data, bool interrupted) override;

	private:
		Vector3 chargeDirection_{};
		float originalDriveForce_ = 0.0f;
		bool directionLocked_ = false;
		bool driveForceOverridden_ = false;
		bool hit_ = false;
	};

	/// 衝撃波のActive開始時に有効距離内のTargetへ一度だけダメージを適用する。
	class ShockwaveAttackBehavior final : public IAttackBehavior
	{
	public:
		/// Factory登録名として衝撃波攻撃種別を返す。
		std::string_view GetTypeName() const override { return "Shockwave"; }
		/// Targetが衝撃波の有効距離内にいるか判定する。
		bool CanStart(const AttackContext& context, const AttackData& data) const override;
		/// 攻撃ごとの一度きり生成フラグを戻す。
		void Begin(AttackContext& context, const AttackData& data) override;
		/// Active開始後の最初の更新だけ衝撃波Damageを適用する。
		AttackExecutionResult Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime) override;
		/// 終了時に個別の生成フラグを破棄する。
		void End(AttackContext& context, const AttackData& data, bool interrupted) override;

	private:
		bool emitted_ = false;
	};

	/// 登録名から個別攻撃クラスを生成し、AttackComponentのJSON復元をswitchから分離する。
	std::unique_ptr<IAttackBehavior> CreateAttackBehavior(std::string_view behaviorType);
} // namespace Ken4lowEngine
