#pragma once

#include "BossAttackTypes.h"

#include <ActorComponent.h>

#include <string>

namespace Ken4lowEngine
{
	class CharacterActor;

	/// BossのTarget追跡、停止、攻撃要求だけを決定し、実処理を各Componentへ委譲するComponent。
	class BossBrainComponent final : public ActorComponent
	{
	public:
		/// Play中に共通移動とBoss攻撃へ判断結果を1回だけ出力する。
		void Update(float deltaTime) override;

		/// 判断状態と調整値をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossBrainComponent"; }

		/// 行動判断の調整値をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから有限な行動調整値を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 追跡対象Actorを設定する。所有権は移さずJSONにも保存しない。
		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }

		/// 死亡時などに判断と移動出力を停止する。
		void StopBehavior();

		/// Debug再検証用に判断状態を初期化する。
		void ResetBehavior();

		/// 現在の判断状態名を返す。
		const std::string& GetStateName() const { return stateName_; }

		/// 現在のTargetまでのXZ距離を返す。
		float GetDistanceToTarget() const { return distanceToTarget_; }

	private:
		CharacterActor* targetActor_ = nullptr;
		float moveSpeed_ = 2.6f;
		float rotateSpeed_ = 5.5f;
		float approachDistance_ = 2.8f;
		float distanceToTarget_ = 0.0f;
		float appliedMoveSpeed_ = 0.0f;
		float appliedRotateSpeed_ = 0.0f;
		BossPhase observedPhase_ = BossPhase::Phase1;
		bool behaviorEnabled_ = true;
		std::string stateName_ = "Idle";
	};
} // namespace Ken4lowEngine
