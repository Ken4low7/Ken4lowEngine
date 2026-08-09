#pragma once

#include <Scene/Actor/Character/AttackComponent.h>

#include <string>

namespace Ken4lowEngine
{
	class CharacterActor;
	class EnemyAIComponent;

	/// 通常敵のScratchとLungeScratchを選択し、攻撃実行を共通AttackComponentへ委譲するComponent。
	class EnemyAttackComponent final : public AttackComponent
	{
	public:
		/// 通常敵の攻撃データを補完し、攻撃タイマーを初期化する。
		void Initialize() override;

		/// 距離・Cooldown・選択確率からScratchまたはLungeScratchを開始する。
		void Update(float deltaTime) override;

		/// 攻撃調整値と実測回数をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyAttackComponent"; }

		/// 攻撃間隔、ダメージ、Lunge選択値をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから攻撃調整値を安全に復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// ダメージ適用先のCharacterActorを共通AttackComponentへ設定する。
		void SetTargetActor(CharacterActor* targetActor) { AttackComponent::SetTargetActor(targetActor); }

		/// AIが現在位置で停止して攻撃開始を待つべきか返す。
		bool ShouldHoldForAttack() const;

		/// Difficulty Directorの攻撃間隔とDamage倍率を登録済み攻撃へ適用する。
		void ApplyDifficultyMultipliers(float cooldownMultiplier, float damageMultiplier);

		/// 死亡中などに攻撃処理を停止する。
		void StopAttacking() { SetAttackEnabled(false); }

		/// 比較再実行用に攻撃タイマーと計測値をリセットする。
		using AttackComponent::ResetAttackState;

		/// Scratch設定の攻撃クールダウンを返す。
		float GetAttackCooldown() const;

		/// Scratchの予備・有効・復帰・Cooldownを含む命中間隔を返す。
		float GetExpectedHitInterval() const;

		/// Scratch設定の1回のダメージを返す。
		float GetAttackDamage() const;

	private:
		/// JSON復元前後のどちらでも通常敵用攻撃が不足しないよう補完する。
		void EnsureDefaultAttacks();
		void EnsureDefaultMeleeAttack();
		void EnsureDefaultLungeAttack();

	private:
		float lungeChance_ = 0.35f;
		float lungeDecisionCooldown_ = 2.0f;
		float lungeDecisionTimer_ = 0.0f;
		float lastLungeRoll_ = 0.0f;
		std::string lastSelectedAttack_ = "None";
	};
} // namespace Ken4lowEngine
