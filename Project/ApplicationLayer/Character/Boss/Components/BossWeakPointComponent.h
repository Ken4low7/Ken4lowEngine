#pragma once

#include <ActorComponent.h>
#include <Scene/Actor/Character/CharacterDamage.h>

#include <string>
#include <string_view>
#include <vector>

namespace Ken4lowEngine
{
	/// HumanoidVisualComponentが所有する部位IDを参照し、Boss弱点倍率だけを管理するComponent。
	class BossWeakPointComponent final : public ActorComponent
	{
	public:
		/// 1つの弱点部位IDとダメージ倍率をデータとして保持する。
		struct WeakPointDefinition
		{
			std::string partId;
			float damageMultiplier = 1.0f;
			bool enabled = true;
		};

	public:
		/// 定義が無い新規Bossへ標準弱点を登録する。
		void Initialize() override;

		/// 弱点定義と参照先部位の状態をDebug表示する。
		void DrawImGui() override;

		/// JSON保存・復元で使用するComponent識別名を返す。
		std::string GetClassTypeName() const override { return "BossWeakPointComponent"; }

		/// 部位ID、倍率、有効状態をActor JSONへ保存する。
		void ToJson(nlohmann::json& outJson) const override;

		/// Actor JSONから弱点定義を復元する。
		void FromJson(const nlohmann::json& inJson) override;

		/// 指定部位IDが有効な弱点なら倍率を、通常部位なら1を返す。
		float ResolveDamageMultiplier(std::string_view partId) const;

		/// 指定部位へのダメージを共通Health経路へ倍率適用して渡す。
		CharacterDamageResult ApplyDamageToPart(std::string_view partId, float baseDamage);

		/// 現在登録されている弱点定義を返す。
		const std::vector<WeakPointDefinition>& GetWeakPoints() const { return weakPoints_; }

		/// HumanVisual内に存在する部位IDだけで構成されているか返す。
		bool HasValidPartReferences() const;

	private:
		/// 重複IDと不正倍率を除外して定義を安全にする。
		void SanitizeDefinitions();

	private:
		std::vector<WeakPointDefinition> weakPoints_;
	};
} // namespace Ken4lowEngine
