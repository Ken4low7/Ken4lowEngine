#pragma once

#include "ActorComponent.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	class CharacterActor;
	class CharacterAnimationComponent;

	/// 通常敵とActor化後のBossが共有する、1種類の攻撃を表す調整データ。
	struct AttackData
	{
		std::string id = "Attack";
		std::string behaviorType = "Melee";
		std::string animationName = "Attack.Melee";
		float damage = 10.0f;
		float cooldown = 0.5f;
		float windupTime = 0.1f;
		float activeTime = 0.1f;
		float recoveryTime = 0.3f;
		float minRange = 0.0f;
		float maxRange = 2.5f;
		float movementSpeed = 0.0f;
		float maxHeightDifference = 2.5f; // XZ距離だけでなく、OwnerとTargetのY差も攻撃可能範囲として制限する。
	};

	/// 個別攻撃へOwner、Target、Animationと3次元距離情報をまとめて渡す実行コンテキスト。
	struct AttackContext
	{
		CharacterActor* owner = nullptr;
		CharacterActor* target = nullptr;
		CharacterAnimationComponent* animation = nullptr;
		float distanceToTarget = 0.0f; // XZ平面上の水平距離。
		float heightDifferenceToTarget = 0.0f; // Owner基準点とTarget基準点の絶対Y差。
	};

	/// 個別攻撃が1フレームで発生させた実行結果を共通イベントへ返す。
	struct AttackExecutionResult
	{
		bool executed = false;
		bool accepted = false;
		float appliedDamage = 0.0f;
	};

	/// 攻撃種別ごとの発動条件と実行だけを分離し、共通タイマー管理へ登録するインターフェース。
	class IAttackBehavior
	{
	public:
		virtual ~IAttackBehavior() = default;

		/// JSON復元で個別処理を再生成するための種別名を返す。
		virtual std::string_view GetTypeName() const = 0;

		/// 共通距離条件以外に攻撃固有の開始条件がある場合に判定する。
		virtual bool CanStart(const AttackContext& context, const AttackData& data) const = 0;

		/// Windup開始時に個別攻撃の一時状態を初期化する。
		virtual void Begin(AttackContext& context, const AttackData& data) = 0;

		/// Active中の個別処理を実行し、命中などが起きたフレームだけ結果を返す。
		virtual AttackExecutionResult Execute(AttackContext& context, const AttackData& data, float deltaTime, float normalizedActiveTime) = 0;

		/// Recovery終了または割り込み時に個別攻撃が残した状態を解除する。
		virtual void End(AttackContext& context, const AttackData& data, bool interrupted) = 0;
	};

	/// 攻撃の開始・実行・終了を外部Effectや検証UIへ通知するイベント種別。
	enum class AttackEventType
	{
		Started,
		Executed,
		Ended,
		Interrupted
	};

	/// 共通攻撃イベントで攻撃ID、対象、実行結果を通知する。
	struct AttackEvent
	{
		AttackEventType type = AttackEventType::Started;
		std::string attackId;
		CharacterActor* owner = nullptr;
		CharacterActor* target = nullptr;
		AttackExecutionResult result{};
	};

	/// 攻撃データ、Cooldown、三段階実行、個別攻撃登録、攻撃イベントを共通管理するComponent。
	class AttackComponent : public ActorComponent
	{
	public:
		using ListenerId = std::uint64_t;
		using AttackListener = std::function<void(const AttackEvent&)>;

		/// 登録済み攻撃を安全な値へ補正し、実行状態を初期化する。
		void Initialize() override;

		/// CooldownとWindup・Active・Recoveryを進め、個別攻撃を一度だけ実行する。
		void Update(float deltaTime) override;

		/// 実行中攻撃と登録データをDetailsへ表示する。
		void DrawImGui() override;

		/// 実行中攻撃を安全に終了し、Listenerと個別攻撃を破棄する。
		void Finalize() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "AttackComponent"; }

		/// 登録攻撃のデータとBehavior種別をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから個別Behaviorを巨大なswitchを使わずFactory登録で復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 個別攻撃処理と調整データをID単位で登録する。
		bool RegisterAttack(AttackData data, std::unique_ptr<IAttackBehavior> behavior);

		/// 指定IDの攻撃が条件とCooldownを満たす場合にWindupを開始する。
		bool StartAttack(std::string_view attackId);

		/// 現在のTargetが指定攻撃の水平距離と高さ差の両方を満たしているか返す。
		bool IsTargetWithinAttackRange(std::string_view attackId) const;

		/// 実行中攻撃を終了処理へ渡し、Interruptedイベントを発行する。
		void InterruptCurrentAttack();

		/// Target Actorを設定する。所有権は移さずJSONにも保存しない。
		void SetTargetActor(CharacterActor* targetActor) { targetActor_ = targetActor; }

		/// 死亡・スタン中などの攻撃開始可否を切り替え、無効化時は実行中攻撃も止める。
		void SetAttackEnabled(bool enabled);

		/// Cooldown、計測値、個別攻撃状態を比較開始時の値へ戻す。
		void ResetAttackState();

		/// 指定IDの攻撃データを返す。
		const AttackData* FindAttackData(std::string_view attackId) const;

		/// 指定IDの攻撃データを上書きし、安全な範囲へ補正する。
		bool ConfigureAttack(std::string_view attackId, const AttackData& data);

		/// 指定IDの残りCooldownを返す。
		float GetCooldownRemaining(std::string_view attackId) const;

		/// 現在攻撃中か返す。
		bool IsAttacking() const { return currentAttackIndex_ < attacks_.size(); }

		/// 現在の攻撃IDを返し、待機中は空文字列を返す。
		std::string_view GetCurrentAttackId() const;

		/// 実際にTargetへ受理された総命中回数を返す。
		int GetAcceptedHitCount() const { return acceptedHitCount_; }

		/// 直近2回の受理命中間隔を返す。
		float GetLastMeasuredInterval() const { return lastMeasuredInterval_; }

		/// 攻撃イベントListenerを登録し、解除用IDを返す。
		ListenerId AddAttackListener(AttackListener listener);

		/// 登録IDに対応する攻撃イベントListenerを解除する。
		bool RemoveAttackListener(ListenerId listenerId);

	protected:
		/// 派生AdapterがTarget参照を確認できるよう読み取り専用で返す。
		CharacterActor* GetTargetActor() const { return targetActor_; }

	private:
		enum class Phase
		{
			Idle,
			Windup,
			Active,
			Recovery
		};

		struct AttackEntry
		{
			AttackData data{};
			std::unique_ptr<IAttackBehavior> behavior;
			float cooldownRemaining = 0.0f;
		};

		struct ListenerEntry
		{
			ListenerId id = 0;
			AttackListener listener;
		};

		/// OwnerとTargetの現在位置から個別攻撃へ渡すContextを生成する。
		AttackContext MakeContext() const;

		/// Contextが指定攻撃の水平距離と高さ差の両方を満たすか判定する。
		static bool IsContextWithinAttackRange(const AttackContext& context, const AttackData& data);

		/// 現在Phaseを1フレーム進め、境界到達時に次Phaseへ遷移する。
		void UpdateCurrentAttack(float deltaTime);

		/// 現在攻撃の終了処理とCooldown開始を一か所で実行する。
		void FinishCurrentAttack(bool interrupted);

		/// 攻撃イベントを登録順のListenerへ安全なコピーで通知する。
		void NotifyAttackEvent(AttackEventType type, const AttackExecutionResult& result = {});

		/// 外部入力された攻撃データを有限かつ実行可能な範囲へ補正する。
		static void SanitizeAttackData(AttackData& data);

	private:
		std::vector<AttackEntry> attacks_;
		std::vector<ListenerEntry> listeners_;
		CharacterActor* targetActor_ = nullptr;
		size_t currentAttackIndex_ = static_cast<size_t>(-1);
		Phase phase_ = Phase::Idle;
		float phaseElapsed_ = 0.0f;
		float elapsedSinceAcceptedHit_ = 0.0f;
		float lastMeasuredInterval_ = 0.0f;
		int acceptedHitCount_ = 0;
		ListenerId nextListenerId_ = 1;
		bool attackEnabled_ = true;
	};
} // namespace Ken4lowEngine
