#pragma once

#include "Vector3.h"

#include <json.hpp>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// 人型を構成する1部位のモデル、親部位、ローカルTransform、表示状態を保持する定義。
	struct HumanoidPartDefinition
	{
		std::string id;
		std::string parentId;
		std::string modelPath;
		Vector3 localPosition{};
		Vector3 localRotation{};
		Vector3 localScale{ 1.0f, 1.0f, 1.0f };
		bool visible = true;
	};

	/// Player、通常Enemy、Bossで共有する人型部位構成をJSON化できるデータクラス。
	class HumanoidDefinition
	{
	public:
		/// 胴体、頭、両腕、両脚を持つ標準的な人型定義を生成する。
		static HumanoidDefinition CreateDefault();

		/// JSONオブジェクトから検証済みの人型定義を復元する。
		bool FromJson(const nlohmann::json& json, std::string* outError = nullptr);

		/// 現在の人型定義を外部定義ファイルと同じJSON形式へ変換する。
		nlohmann::json ToJson() const;

		/// 指定JSONファイルから人型定義を安全に読み込む。
		bool LoadFromFile(std::string_view filePath, std::string* outError = nullptr);

		/// 現在の人型定義を指定JSONファイルへ保存する。
		bool SaveToFile(std::string_view filePath, std::string* outError = nullptr) const;

		/// ID重複、存在しない親、循環参照を含まない定義か検証する。
		bool Validate(std::string* outError = nullptr) const;

		/// 指定IDに一致する部位定義を返す。
		HumanoidPartDefinition* FindPart(std::string_view partId);

		/// 指定IDに一致する部位定義を返すconst版。
		const HumanoidPartDefinition* FindPart(std::string_view partId) const;

		/// 人型を構成する全部位の定義を返す。
		const std::vector<HumanoidPartDefinition>& GetParts() const { return parts_; }

		/// 呼び出し側で組み立てた部位定義一覧を設定する。
		void SetParts(std::vector<HumanoidPartDefinition> parts) { parts_ = std::move(parts); }

	private:
		std::vector<HumanoidPartDefinition> parts_;
	};
} // namespace Ken4lowEngine
