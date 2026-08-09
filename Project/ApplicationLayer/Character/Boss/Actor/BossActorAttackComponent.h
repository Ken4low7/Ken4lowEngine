#pragma once

#include "BossAttackTypes.h"

#include <Scene/Actor/Character/AttackComponent.h>

#include <string>
#include <string_view>

namespace Ken4lowEngine
{
	/// Boss用の複数攻撃定義とフェーズ別選択を共通AttackComponentへ追加するComponent。
	class BossAttackComponent final : public AttackComponent
	{
	public:
		/// JSON復元済み攻撃プロファイルに応じたBoss攻撃を登録する。
		void Initialize() override;

		/// 共通攻撃状態にBossの直近選択結果を追加表示する。
		void DrawImGui() override;

		/// 攻撃プロファイルと共通攻撃データをJSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;
		void FromJson(const nlohmann::json& inJson) override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossAttackComponent"; }

		/// 距離と現在フェーズに適した登録攻撃を優先順に開始する。
		bool TryStartBestAttack(float distanceToTarget, BossPhase bossPhase);

		/// 現在プロファイルの全フェーズ・全距離帯に有効な候補が登録されているか検証する。
		bool ValidateSelectionRules(std::string& outSummary) const;

		/// 直近に開始できた攻撃を型付きIDで返す。
		BossAttackId GetLastSelectedAttackType() const { return lastSelectedAttackId_; }

		/// 既存Debug表示との互換用に攻撃ID文字列を返す。
		std::string GetLastSelectedAttackId() const { return std::string(ToString(lastSelectedAttackId_)); }
		std::string_view GetLastSelectedAttackName() const { return ToString(lastSelectedAttackId_); }
		BossAttackProfile GetAttackProfile() const { return attackProfile_; }
		std::string_view GetAttackProfileName() const { return ToString(attackProfile_); }

	private:
		/// 選択されたArchetypeに対応する近接・突進・範囲攻撃を登録する。
		void RegisterDefaultAttacks();
		void RegisterGuardianAttacks();
		void RegisterMineCrusherAttacks();

	private:
		BossAttackProfile attackProfile_ = BossAttackProfile::Guardian;
		BossAttackId lastSelectedAttackId_ = BossAttackId::None;
	};
} // namespace Ken4lowEngine
