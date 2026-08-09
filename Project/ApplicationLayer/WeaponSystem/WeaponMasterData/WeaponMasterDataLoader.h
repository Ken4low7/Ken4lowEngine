#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>

#include "WeaponMasterData.h"

/// -------------------------------------------------------------
///				　		武器マスターデータローダー
/// -------------------------------------------------------------
class WeaponMasterDataLoader
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定したJSONファイルから武器マスターデータを読み込み、outに格納します。読み込みまたは解析に失敗した場合はfalseを返し、outErrorが指定されていればエラーメッセージを設定します。
	/// </summary>
	/// <param name="jsonPath">読み込むJSONファイルへのパス。</param>
	/// <param name="out">読み込んだデータを格納する出力参照（FWeaponMasterData）。</param>
	/// <param name="outError">オプションのエラーメッセージ受け取り用ポインタ。エラー発生時に説明を格納します。省略可能（デフォルトはnullptr）。</param>
	/// <returns>読み込みと解析に成功した場合はtrue、失敗した場合はfalseを返します。</returns>
	static bool LoadFromFile(const std::filesystem::path& jsonPath, FWeaponMasterData& out, std::string* outError = nullptr);

	/// <summary>
	/// 指定したディレクトリからすべての武器マスターデータを読み込み、outMap に格納します。処理に失敗した場合は false を返し、outError が指定されていればエラーメッセージが設定されます。
	/// </summary>
	/// <param name="dirPath">読み込むファイルを含むディレクトリのパス。</param>
	/// <param name="outMap">読み込んだ FWeaponMasterData を格納する出力マップ。キーは int32_t の識別子、値は対応する FWeaponMasterData。</param>
	/// <param name="outError">エラーメッセージを受け取るオプションの出力ポインタ。nullptr の場合はエラーメッセージは返されません。</param>
	/// <returns>読み込みに成功したら true、失敗したら false を返します。</returns>
	static bool LoadAllFromDirectory(const std::filesystem::path& dirPath, std::unordered_map<int32_t, FWeaponMasterData>& outMap, std::string* outError = nullptr);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// エラーメッセージを設定し、失敗を示す false を返す。
	/// </summary>
	/// <param name="outError">エラー出力先の std::string へのポインタ。nullptr の場合は何も書き込まれない。</param>
	/// <param name="msg">設定するエラーメッセージの内容（std::string_view）。</param>
	/// <returns>失敗を示す false。通常 msg は outError に書き込まれる。</returns>
	static bool Fail(std::string* outError, std::string_view msg);
};

