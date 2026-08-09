#pragma once
#include <filesystem>
#include <string>
#include "WeaponMasterDataDatabase.h"

/// -------------------------------------------------------------
///				　		武器マスターデータライター
/// -------------------------------------------------------------
class WeaponMasterDataWriter
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 武器マスターデータベース内の全データをカテゴリ別に rootDir 配下へ保存します。
	/// </summary>
	/// <param name="database">保存対象となる武器マスターデータを保持するデータベースへの参照（読み取り専用）。</param>
	/// <param name="rootDir">保存先のルートディレクトリのパス。カテゴリごとのサブディレクトリやファイルがこの配下に作成されます。</param>
	/// <param name="outError">省略可能。失敗時にエラー内容の説明を格納するための出力パラメータ。渡さない（nullptr）の場合はエラー文字列は返されません。</param>
	/// <returns>処理が成功したら true、失敗したら false を返します。失敗時は outError に詳細が設定されることがあります。</returns>
	static bool SaveAllByCategory(const WeaponMasterDataDatabase& database, const std::filesystem::path& rootDir, std::string* outError = nullptr);

	/// <summary>
	/// 指定されたファイルパスに武器マスターデータを保存する。エラーが発生した場合は outError にエラーメッセージが設定されることがある。
	/// </summary>
	/// <param name="filePath">保存先のファイルパスを示す参照。</param>
	/// <param name="data">保存する武器マスターデータへの参照。</param>
	/// <param name="outError">オプションの出力引数。エラー発生時にエラーメッセージを格納するための std::string ポインタ（nullptr 可）。</param>
	/// <returns>保存に成功した場合は true、失敗した場合は false を返す。</returns>
	static bool SaveOne(const std::filesystem::path& filePath, const FWeaponMasterData& data, std::string* outError = nullptr);

	static std::filesystem::path MakeWeaponFilePath(const std::filesystem::path& rootDir, const FWeaponMasterData& data);

	static std::string SanitizeFileStem(std::string s);

	/// <summary>
	/// 指定したルートディレクトリ内で、指定した武器IDに対応するファイルを削除します。
	/// </summary>
	/// <param name="rootDir">検索および削除を行うルートディレクトリのパス。</param>
	/// <param name="weaponID">削除対象となる武器の識別子（ID）。</param>
	/// <param name="outError">オプションの出力先。操作中にエラーが発生した場合、このポインタにエラーメッセージが格納されます。nullptr を渡すとエラーメッセージは受け取りません。</param>
	/// <returns>削除処理が成功した場合は true、失敗した場合は false を返します。失敗時は outError に詳細が設定される可能性があります。</returns>
	static bool DeleteFilesByWeaponID(const std::filesystem::path& rootDir, int32_t weaponID, std::string* outError = nullptr);

	static bool DeleteFilesForWeapon(const std::filesystem::path& rootDir, const FWeaponMasterData& data, std::string* outError = nullptr);
};

