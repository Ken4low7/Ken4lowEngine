#pragma once
#include <string>

#include "WeaponMasterData.h"

/// -------------------------------------------------------------
///				　武器マスターデータバリデーター
/// -------------------------------------------------------------
class WeaponMasterDataValidator
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定した武器マスターデータが有効かどうかを検証します。無効な場合は false を返し、outError が指定されていればエラーメッセージを設定します。
	/// </summary>
	/// <param name="data">検証する FWeaponMasterData の参照。</param>
	/// <param name="outError">オプションのエラーメッセージ受け取り用ポインタ。無効な場合に説明を格納します。省略可能（デフォルトは nullptr）。</param>
	/// <returns>データが有効な場合は true、無効な場合は false を返します。</returns>
	static bool Validate(const FWeaponMasterData& data, std::string* outError = nullptr);

private: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// 指定されたエラーメッセージを出力文字列に追加します。outError は有効な std::string へのポインタである必要があります。
	/// </summary>
	/// <param name="outError">結果のエラーメッセージを格納する std::string へのポインタ。追加先の文字列であり、nullptr であってはなりません。</param>
	/// <param name="msg">outError に追加するメッセージ（参照）。</param>
	static void Append(std::string* outError, const std::string& msg);
};

