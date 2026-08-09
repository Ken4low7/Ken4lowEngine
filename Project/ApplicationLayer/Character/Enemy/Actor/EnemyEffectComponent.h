#pragma once

#include "ApplicationLayer/Character/Enemy/Effects/EnemyParticleEffectSystem.h"

#include <ActorComponent.h>
#include <Vector3.h>
#include <Vector4.h>

#include <string>

namespace Ken4lowEngine
{
	class HumanoidVisualComponent;

	/// 通常敵の生成・被弾・死亡Effectと表示アニメーションを、EnemyActor本体から分離するComponent。
	class EnemyEffectComponent final : public ActorComponent
	{
	public:
		/// 共有Enemy Effect Systemを一度だけ初期化し、最初の更新で生成演出を開始する。
		void Initialize() override;

		/// 生成時の拡大・発光と死亡後の表示終了を進める。
		void Update(float deltaTime) override;

		/// Effect発生回数と生成・死亡演出状態をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "EnemyEffectComponent"; }

		/// 生成・死亡表示時間をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから演出時間を安全に復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 指定位置を中心に生成パーティクルと拡大表示を開始する。
		void TriggerSpawnEffect(const Vector3& worldPosition);

		/// 被弾位置へ旧通常敵と同じHit Effectを発生させる。
		void TriggerHitEffect(const Vector3& worldPosition);

		/// 死亡位置へ旧通常敵と同じDeath Effectを発生させ、表示終了タイマーを開始する。
		void TriggerDeathEffect(const Vector3& worldPosition);

		/// 比較再実行用に表示、色、Effect計測値を初期状態へ戻す。
		void ResetEffectState();

		int GetSpawnEffectCount() const { return spawnEffectCount_; }
		int GetDeathEffectCount() const { return deathEffectCount_; }
		int GetHitEffectCount() const { return hitEffectCount_; }
		bool IsSpawnEffectActive() const { return spawnEffectActive_; }
		bool IsDeathEffectActive() const { return deathEffectActive_; }

	private:
		/// Ownerの現在位置を取得し、初回生成演出を遅延開始する。
		void StartPendingSpawnEffect();

		/// 生成時のスケール、色、発光を経過率から更新する。
		void UpdateSpawnPresentation(float deltaTime);

		/// 生成演出を終了し、元の見た目と無敵状態へ戻す。
		void FinishSpawnPresentation();

		/// 人型の胴体を含む全部位へ同じ色と発光を適用する。
		void ApplyVisualColor(HumanoidVisualComponent& visual, const Vector4& color, const Vector4& emissive);

	private:
		EnemyParticleEffectSystem effectSystem_{};
		Vector3 spawnBaseScale_{ 1.0f, 1.0f, 1.0f };
		float spawnPresentationDuration_ = 0.55f;
		float spawnTimer_ = 0.0f;
		float deathPresentationDuration_ = 1.8f;
		float deathTimer_ = 0.0f;
		int spawnEffectCount_ = 0;
		int hitEffectCount_ = 0;
		int deathEffectCount_ = 0;
		bool spawnPresentationPending_ = true;
		bool spawnEffectActive_ = false;
		bool spawnPreviousInvulnerable_ = false;
		bool deathEffectActive_ = false;
	};
} // namespace Ken4lowEngine
