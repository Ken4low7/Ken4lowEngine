#pragma once
#include <string>

namespace Ken4lowEngine
{

	/// <summary>
	/// Visual Studio の出力ウィンドウへデバッグ文字列を送るためのログ関数です。
	/// ゲーム処理中の状態確認や、初期化失敗箇所の簡易確認に使用します。
	/// </summary>
	// 末尾の改行は呼び出し側で必要に応じて付ける。
	void Log(const std::string& message);

	/// <summary>
	/// UTF-8 の std::string を Windows API で扱いやすい std::wstring に変換します。
	/// </summary>
	/// <param name="str">変換元の UTF-8 文字列。</param>
	/// <returns>変換後のワイド文字列。変換できない場合は空文字列を返します。</returns>
	std::wstring ConvertString(const std::string& str);

	/// <summary>
	/// std::wstring を UTF-8 の std::string に変換します。
	/// </summary>
	/// <param name="str">変換元のワイド文字列。</param>
	/// <returns>変換後の UTF-8 文字列。変換できない場合は空文字列を返します。</returns>
	std::string ConvertString(const std::wstring& str);

} // namespace Ken4lowEngine
