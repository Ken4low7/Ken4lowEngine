#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

#include <json.hpp>

#include <cstddef>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>
	/// JSONの欠損キーや型不正を既定値へ安全にフォールバックする読み取りUtilityです。
	/// 各Serializerに散っていた value / contains / Vector配列チェックを集約し、既存Json互換を保ったまま読み取り処理を見通しよくします。
	/// </summary>
	class JsonReadUtil
	{
	public:
		/// <summary>
		/// 指定キーの値を読み取ります。
		/// 欠損キーや型不正は古いJsonとの互換のため失敗扱いにし、呼び出し側の既定値を維持できるようfalseを返します。
		/// </summary>
		template<class T>
		static bool TryRead(const nlohmann::json& json, const char* key, T& outValue)
		{
			const auto it = json.find(key);
			if (it == json.end())
			{
				return false;
			}

			try
			{
				outValue = it->get<T>();
				return true;
			}
			catch (const nlohmann::json::exception&)
			{
				return false;
			}
		}

		/// <summary>指定キーがオブジェクトとして存在するかを返します。</summary>
		static bool ContainsObject(const nlohmann::json& json, const char* key)
		{
			const auto it = json.find(key);
			return it != json.end() && it->is_object();
		}

		/// <summary>文字列を読み取ります。欠損・型不正の場合は既定値を返して既存Json互換を保ちます。</summary>
		static std::string ReadStringOr(const nlohmann::json& json, const char* key, const std::string& defaultValue)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_string())
			{
				return it->get<std::string>();
			}
			return defaultValue;
		}

		/// <summary>floatを読み取ります。欠損・型不正の場合は既定値を返して調整データの途中保存にも耐えます。</summary>
		static float ReadFloatOr(const nlohmann::json& json, const char* key, float defaultValue)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_number())
			{
				return it->get<float>();
			}
			return defaultValue;
		}

		/// <summary>intを読み取ります。欠損・型不正の場合は既定値を返します。</summary>
		static int ReadIntOr(const nlohmann::json& json, const char* key, int defaultValue)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_number_integer())
			{
				return it->get<int>();
			}
			return defaultValue;
		}

		/// <summary>boolを読み取ります。欠損・型不正の場合は既定値を返します。</summary>
		static bool ReadBoolOr(const nlohmann::json& json, const char* key, bool defaultValue)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_boolean())
			{
				return it->get<bool>();
			}
			return defaultValue;
		}

		/// <summary>オブジェクトを読み取ります。欠損・型不正の場合は既定値を返し、旧Jsonの構造差を吸収します。</summary>
		static nlohmann::json ReadObjectOr(const nlohmann::json& json, const char* key, const nlohmann::json& defaultValue)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_object())
			{
				return *it;
			}
			return defaultValue;
		}

		/// <summary>配列を読み取ります。欠損・型不正の場合は既定値を返します。</summary>
		static nlohmann::json ReadArrayOr(const nlohmann::json& json, const char* key, const nlohmann::json& defaultValue)
		{
			const auto it = json.find(key);
			if (it != json.end() && it->is_array())
			{
				return *it;
			}
			return defaultValue;
		}

		/// <summary>Vector2配列を読み取ります。要素順は既存Json互換のため x, y のまま維持します。</summary>
		static Vector2 ReadVector2Or(const nlohmann::json& json, const char* key, const Vector2& defaultValue)
		{
			const auto* array = FindNumberArray(json, key, 2);
			if (!array)
			{
				return defaultValue;
			}
			return { (*array)[0].get<float>(), (*array)[1].get<float>() };
		}

		/// <summary>Vector3配列を読み取ります。要素順は既存Json互換のため x, y, z のまま維持します。</summary>
		static Vector3 ReadVector3Or(const nlohmann::json& json, const char* key, const Vector3& defaultValue)
		{
			const auto* array = FindNumberArray(json, key, 3);
			if (!array)
			{
				return defaultValue;
			}
			return { (*array)[0].get<float>(), (*array)[1].get<float>(), (*array)[2].get<float>() };
		}

		/// <summary>Vector4配列を読み取ります。要素順は既存Json互換のため x, y, z, w のまま維持します。</summary>
		static Vector4 ReadVector4Or(const nlohmann::json& json, const char* key, const Vector4& defaultValue)
		{
			const auto* array = FindNumberArray(json, key, 4);
			if (!array)
			{
				return defaultValue;
			}
			return { (*array)[0].get<float>(), (*array)[1].get<float>(), (*array)[2].get<float>(), (*array)[3].get<float>() };
		}

	private:
		/// <summary>Vector読み取り用に、指定キーが必要数以上の数値配列かを確認します。</summary>
		static const nlohmann::json* FindNumberArray(const nlohmann::json& json, const char* key, size_t requiredSize)
		{
			const auto it = json.find(key);
			if (it == json.end() || !it->is_array() || it->size() < requiredSize)
			{
				return nullptr;
			}
			for (size_t index = 0; index < requiredSize; ++index)
			{
				if (!(*it)[index].is_number())
				{
					return nullptr;
				}
			}
			return &(*it);
		}
	};
} // namespace Ken4lowEngine
