#pragma once

#include "ActorComponent.h"
#include "CharacterDamage.h"
#include "ComponentProperty.h"

#include <vector>

namespace Ken4lowEngine
{
	/// HP計算、生存判定、無敵状態をCharacterActorから分離して管理するComponent。
	class CharacterHealthComponent : public ActorComponent
	{
	public:
		/// JSON復元値を安全なHP範囲へ補正する。
		void Initialize() override;

		/// Character用HPをDetails上で編集・確認する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponentクラス名を返す。
		std::string GetClassTypeName() const override { return "CharacterHealthComponent"; }

		/// HP設定をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONからHP設定を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// ダメージ要求を検証し、HPを0未満にしない範囲で適用する。
		CharacterDamageResult ApplyDamage(const CharacterDamageInfo& damageInfo);

		/// 生存中のHPを最大値以内で回復し、実際の回復量を返す。
		float Heal(float amount);

		/// 最大HPと現在HPを同時に設定し、再利用可能な生存状態へ戻す。
		void ResetHealth(float maxHealth);

		/// 現在の最大HPまで回復して死亡状態から復帰させる。
		void RestoreFullHealth();

		/// 最大HPを1以上へ補正し、現在HPも新しい上限内へ収める。
		void SetMaxHealth(float maxHealth);

		/// 現在HPを0から最大HPまでの範囲へ補正する。
		void SetCurrentHealth(float currentHealth);

		/// ダメージを受け付けない状態を切り替える。
		void SetInvulnerable(bool invulnerable) { isInvulnerable_ = invulnerable; }

		/// 現在HPが残っているか返す。
		bool IsAlive() const { return currentHealth_ > 0.0f; }

		/// 現在HPが0になっているか返す。
		bool IsDead() const { return !IsAlive(); }

		/// 現在HPを返す。
		float GetCurrentHealth() const { return currentHealth_; }

		/// 最大HPを返す。
		float GetMaxHealth() const { return maxHealth_; }

		/// 0から1までのHP割合を返す。
		float GetHealthRatio() const;

		/// 現在無敵状態か返す。
		bool IsInvulnerable() const { return isInvulnerable_; }

	private:
		/// JSONとDetailsで共有する編集プロパティ一覧を生成する。
		std::vector<ComponentProperty> CreateProperties();

	private:
		float maxHealth_ = 100.0f;
		float currentHealth_ = 100.0f;
		bool isInvulnerable_ = false;
	};
} // namespace Ken4lowEngine
