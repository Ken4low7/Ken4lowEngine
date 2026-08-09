#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <functional>
#include <json.hpp>
#include <string>
#include <variant>
#include <vector>

namespace Ken4lowEngine
{
	/// ---------- コンポーネントのプロパティ種別 ---------- ///
	enum class ComponentPropertyType
	{
		Bool,	 // 真偽値
		Int,	 // 整数
		Float,	 // 浮動小数点数
		String,	 // 文字列
		Vector2, // 2次元ベクトル
		Vector3, // 3次元ベクトル
		Vector4, // 4次元ベクトル
	};

	enum class ComponentPropertyDisplay
	{
		Default,
		Color,
		MultilineText,
	};

	// ComponentPropertyValue は、コンポーネントのプロパティの値を表すために使用される
	using ComponentPropertyValue = std::variant<bool, int, float, std::string, Vector2, Vector3, Vector4>;

	struct ComponentPropertyChoice
	{
		std::string value;
		std::string displayName;
	};

	/// ---------- コンポーネントのプロパティ情報 ---------- ///
	struct ComponentProperty
	{
		std::string name;										   // プロパティの内部名
		std::string displayName;								   // プロパティの表示名
		ComponentPropertyType type = ComponentPropertyType::Float; // プロパティの型
		std::function<ComponentPropertyValue()> getter;			   // プロパティの値を取得するための関数
		std::function<void(const ComponentPropertyValue&)> setter; // プロパティの値を設定するための関数
		float min = 0.0f;										   // プロパティの最小値
		float max = 0.0f;										   // プロパティの最大値
		float speed = 0.1f;										   // プロパティの変化速度
		bool hasRange = false;									   // プロパティに範囲があるかどうか
		std::vector<ComponentPropertyChoice> choices;			   // 選択式プロパティの候補
		ComponentPropertyDisplay display = ComponentPropertyDisplay::Default; // ImGui上の表示方法
	};

	/// -------------------------------------------------------------
	///	  コンポーネントのプロパティを操作するユーティリティクラス
	/// -------------------------------------------------------------
	class ComponentPropertyUtility
	{
	public: /// ---------- 静的メンバ関数 ---------- ///

		static bool DrawImGui(const std::vector<ComponentProperty>& properties);
		static void ToJson(const std::vector<ComponentProperty>& properties, nlohmann::json& outJson);
		static void FromJson(const std::vector<ComponentProperty>& properties, const nlohmann::json& inJson);
	};
}
