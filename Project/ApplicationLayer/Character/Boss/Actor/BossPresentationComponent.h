#pragma once

#include <ActorComponent.h>
#include <Object3D.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	class BossAttackComponent;
	class CharacterActor;
	struct AttackEvent;

	/// フェーズ遷移、攻撃予兆、死亡の見せ方をBrainや攻撃ロジックから分離するComponent。
	class BossPresentationComponent final : public ActorComponent
	{
	private:
		struct AfterimagePart
		{
			std::string partId;
			std::unique_ptr<Object3D> object;
			Matrix4x4 worldMatrix{};
			bool visible = false;
		};

		struct AfterimageSnapshot
		{
			std::vector<AfterimagePart> parts;
			float age = 0.0f;
			bool active = false;
		};

	public:
		/// 現在Phase Revisionを初期値として保持し、生成直後の不要な演出を防ぐ。
		void Initialize() override;

		/// Phase変更、攻撃予兆、Aura、攻撃着地Camera Shakeを更新する。
		void Update(float deltaTime) override;

		/// 本体描画前に固定姿勢の半透明残像を古い順で描画する。
		void Draw() override;

		/// 攻撃Listenerを解除し、古いAttack Componentへの参照を残さない。
		void Finalize() override;

		/// 演出状態、攻撃予兆、残り時間をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossPresentationComponent"; }

		/// 演出時間設定をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから演出時間設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 死亡時の攻撃中断と死亡ポーズを開始する。
		void StartDeathPresentation();

		/// Brainが行動を止めるべき演出中か返す。
		bool IsBlockingBehavior() const { return phaseTransitionActive_ || deathPresentationActive_; }

		/// 死亡演出が指定時間まで進んだか返す。
		bool IsDeathPresentationComplete() const { return deathPresentationActive_ && elapsed_ >= deathPresentationDuration_; }

		/// 現在の演出状態名を返す。
		const std::string& GetStateName() const { return stateName_; }

		/// Debug再検証用に演出状態を初期化する。
		void ResetPresentation();

	private:
		/// JSON再読込後も現在のAttack ComponentへListenerを張り直す。
		void EnsureAttackListener();

		/// 攻撃イベントを予兆開始、着地演出、終了へ振り分ける。
		void HandleAttackEvent(const AttackEvent& event);

		/// Windup時間を使って攻撃固有の予兆を開始する。
		void StartAttackTelegraph(const AttackEvent& event);

		/// 予兆の明滅とパーティクル発生を進める。
		void UpdateAttackTelegraph(float deltaTime);

		/// 攻撃終了・割り込み時に予兆状態を解除する。
		void FinishAttackTelegraph();

		/// 攻撃IDに応じた予兆または着地パーティクルを発生させる。
		void EmitAttackTelegraphPulse(bool impact);

		/// 現在のBoss位置からTargetへ向かうXZ方向を返す。
		Vector3 ResolveCurrentTargetDirection() const;

		/// 突進Active時間と固定方向を使う疾走演出を開始する。
		void BeginChargeTrail(float activeDuration);

		/// 突進中の大型方向ガイドと残像生成を進める。
		void UpdateChargeTrail(float deltaTime);

		/// 突進終了時に追加生成だけを止め、既存残像は寿命まで残す。
		void StopChargeTrail();

		/// 人型定義を使って残像用Object3Dを事前生成する。
		void EnsureAfterimagePool();

		/// 現在の全Body Part姿勢を空き残像枠へ固定保存する。
		void SpawnChargeAfterimage();

		/// 残像のAlphaと発光を寿命に合わせて更新する。
		void UpdateAfterimages(float deltaTime);

		/// 残像を全て停止し、再戦時に前回の見た目を残さない。
		void ResetAfterimages();

		/// Phase変更直後と継続Auraのパーティクルを発生させる。
		void EmitPhasePulse(bool initialBurst);

		/// Phase、予兆、死亡状態から全部位の色と発光を更新する。
		void ApplyVisualAppearance();

		/// Phase遷移中の攻撃を止め、専用Animationを開始する。
		void StartPhaseTransition(int phase);

		/// 範囲攻撃・突進命中・死亡時のCamera Shakeを開始する。
		void StartCameraShake(float duration, float amplitude, float frequency);

		/// 現在のPlayer Cameraへ減衰するShake Offsetを加える。
		void UpdateCameraShake(float deltaTime);

	private:
		BossAttackComponent* boundAttack_ = nullptr;
		CharacterActor* telegraphTarget_ = nullptr;
		std::uint64_t attackListenerId_ = 0;
		unsigned int observedPhaseRevision_ = 0;
		int presentedPhase_ = 1;
		float phaseTransitionDuration_ = 0.50f;
		float deathPresentationDuration_ = 1.2f;
		float telegraphParticleInterval_ = 0.10f;
		float phaseAuraInterval_ = 0.18f;
		float elapsed_ = 0.0f;
		float visualTime_ = 0.0f;
		float telegraphElapsed_ = 0.0f;
		float telegraphDuration_ = 0.0f;
		float telegraphParticleTimer_ = 0.0f;
		float phaseParticleTimer_ = 0.0f;
		float chargeTrailElapsed_ = 0.0f;
		float chargeTrailDuration_ = 0.0f;
		float chargeGuideTimer_ = 0.0f;
		float afterimageSpawnTimer_ = 0.0f;
		float cameraShakeTimer_ = 0.0f;
		float cameraShakeDuration_ = 0.0f;
		float cameraShakeAmplitude_ = 0.0f;
		float cameraShakeFrequency_ = 0.0f;
		float cameraShakeSeed_ = 0.0f;
		Vector3 chargeDirection_{ 0.0f, 0.0f, 1.0f };
		std::vector<AfterimageSnapshot> afterimagePool_;
		std::size_t nextAfterimageIndex_ = 0;
		bool phaseTransitionActive_ = false;
		bool deathPresentationActive_ = false;
		bool attackTelegraphActive_ = false;
		bool chargeTrailActive_ = false;
		bool chargeDirectionLocked_ = false;
		std::string activeAttackId_;
		std::string stateName_ = "Idle";
	};
} // namespace Ken4lowEngine
