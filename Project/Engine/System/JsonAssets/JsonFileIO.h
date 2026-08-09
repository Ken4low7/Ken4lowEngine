#pragma once

#include <json.hpp>

#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

namespace Ken4lowEngine
{
	/// <summary>
	/// JSONファイルの読み込み・保存だけを担当するUtilityです。
	/// Serializerごとに重複していた ifstream / ofstream / dump(indent) を集約し、保存形式は呼び出し側が明示したindentで維持します。
	/// </summary>
	class JsonFileIO
	{
	public:
		/// <summary>
		/// JSONファイルを読み込みます。
		/// ファイルが無い、開けない、パースできない場合はfalseを返し、既存の読み込み失敗時挙動を呼び出し側で維持できるようにします。
		/// </summary>
		static bool LoadJsonFile(const std::string& filePath, nlohmann::json& outJson)
		{
			try
			{
				std::ifstream input(filePath);
				if (!input.is_open())
				{
					return false;
				}
				input >> outJson;
				return true;
			}
			catch (const std::exception&)
			{
				return false;
			}
		}

		/// <summary>
		/// JSONファイルを保存します。
		/// 既存Json互換のため構造は変更せず、整形幅は呼び出し側が既存値と同じ値を渡します。
		/// </summary>
		static bool SaveJsonFile(const std::string& filePath, const nlohmann::json& json, int indent = 4)
		{
			try
			{
				const std::filesystem::path path(filePath);
				if (path.has_parent_path())
				{
					std::filesystem::create_directories(path.parent_path());
				}

				std::ofstream output(filePath);
				if (!output.is_open())
				{
					return false;
				}

				output << json.dump(indent);
				return output.good();
			}
			catch (const std::exception&)
			{
				return false;
			}
		}
	};
} // namespace Ken4lowEngine
