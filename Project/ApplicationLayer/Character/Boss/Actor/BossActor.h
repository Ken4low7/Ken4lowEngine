#pragma once

#include <Scene/Actor/Character/CharacterActor.h>

#include <string_view>

namespace Ken4lowEngine
{
	class BossAttackComponent;
	class BossBrainComponent;
	class BossPhaseComponent;
	class BossPresentationComponent;
	class BossWeakPointComponent;
	class GaugeComponent;
	class HumanoidVisualComponent;
	class Matrix4x4;
	class TextComponent;

	/// 共通Character機能とBoss専用判断・攻撃・フェーズ・弱点・演出・HP HUDを束ねる本番Actor。
	class BossActor final : public CharacterActor
	{
	public:
		/// Boss専用Componentを不足分だけ生成し、共通Character Componentへ接続する。
		void Initialize() override;

		/// 共通更新後にBoss Healthを画面上部Gaugeへ同期し、死亡位置を固定する。
		void Update(float deltaTime) override;

		/// Physics補正後も死亡地点を維持し、崩壊演出が別座標へ飛ばないようにする。
		void PostPhysicsUpdate(float deltaTime) override;

		/// JSON保存・復元で使用するActor識別名を返す。
		std::string GetClassTypeName() const override { return "BossActor"; }

		/// BrainとAttackへ同じ追跡対象を設定する。
		void SetTargetActor(CharacterActor* targetActor);

		/// 弱点部位IDを解決し、倍率適用後のダメージを共通Healthへ渡す。
		CharacterDamageResult ApplyWeakPointDamage(std::string_view partId, float baseDamage);

		/// 弾・Splash Damage用に命中位置を含むDamageを共通Healthへ渡す。
		CharacterDamageResult ApplyBulletDamage(float damage, const Vector3& hitPosition);
		CharacterDamageResult ApplyBulletDamage(const CharacterDamageInfo& damageInfo) { return ApplyDamage(damageInfo); } // 弾・爆発とも同じDamage構造をBoss Healthへ渡す。

		/// DebugSceneで同じ個体を再検証できる初期状態へ戻す。
		void ResetForValidation(const Vector3& worldPosition);

		/// 登場演出と本編の両方からBossのWorld座標を設定する。
		void SetPosition(const Vector3& worldPosition);
		Vector3 GetPosition() const;
		void SetYaw(float yaw);

		/// Intro終了時にRoot親子関係を外してWorld座標を維持する。
		void ClearRootParentKeepingWorldPosition();
		void ForceSyncWorldTransform();
		bool HasRootParent() const;
		Vector3 GetRootLocalPosition() const;
		Vector3 GetRootWorldPosition() const;

		/// ActorWorld所有のまま、Intro中と戦闘中のAI・物理・Colliderを切り替える。
		void SetBattleEnabled(bool enabled);
		bool IsBattleEnabled() const { return battleEnabled_; }

		/// Actor所有のBoss HP HUDを必要な区間だけ表示する。
		void SetHealthHudVisible(bool visible);

		/// Shadow描画に使用するライト行列をVisual Componentへ渡す。
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);

		float GetHP() const;
		float GetMaxHP() const;
		int GetCurrentPhase() const;
		unsigned int GetPhaseRevision() const;
		bool IsDeathPresentationComplete() const;
		const Vector3& GetDeathWorldPosition() const { return deathWorldPosition_; }

		/// Bossの行動判断Componentを返す。
		BossBrainComponent* GetBossBrainComponent();
		const BossBrainComponent* GetBossBrainComponent() const;

		/// Bossの攻撃選択と共通攻撃実行Componentを返す。
		BossAttackComponent* GetBossAttackComponent();
		const BossAttackComponent* GetBossAttackComponent() const;

		/// HP割合からフェーズを判定するComponentを返す。
		BossPhaseComponent* GetBossPhaseComponent();
		const BossPhaseComponent* GetBossPhaseComponent() const;

		/// 部位ID参照で弱点倍率を管理するComponentを返す。
		BossWeakPointComponent* GetBossWeakPointComponent();
		const BossWeakPointComponent* GetBossWeakPointComponent() const;

		/// Phase遷移と死亡演出を管理するComponentを返す。
		BossPresentationComponent* GetBossPresentationComponent();
		const BossPresentationComponent* GetBossPresentationComponent() const;

		/// Bossの全部位描画を担当するComponentを返す。
		HumanoidVisualComponent* GetHumanoidVisualComponent();
		const HumanoidVisualComponent* GetHumanoidVisualComponent() const;

		/// 画面上部のBoss HP Gaugeを返す。
		GaugeComponent* GetHealthGaugeComponent();
		const GaugeComponent* GetHealthGaugeComponent() const;

		/// Boss HPラベルを返す。
		TextComponent* GetHealthLabelComponent();
		const TextComponent* GetHealthLabelComponent() const;

	protected:
		/// 死亡時は判断・攻撃・移動・Colliderを止め、撃破地点で死亡演出を開始する。
		void OnDeath(const CharacterDeathEvent& deathEvent) override;

	private:
		/// CharacterHealthの現在値を画面固定Gaugeへ反映する。
		void SyncHealthHud();
		void RestoreDeathWorldPosition();

	private:
		CharacterActor* targetActor_ = nullptr;
		Vector3 deathWorldPosition_{};
		bool battleEnabled_ = false;
		bool hasDeathWorldPosition_ = false;
	};
} // namespace Ken4lowEngine
