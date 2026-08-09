#include "LogString.h"

#include <Windows.h>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                 デバッグログ出力処理
	/// -------------------------------------------------------------
	void Log(const std::string& message)
	{
		// Visual Studio の出力ウィンドウへそのまま文字列を流す。
		OutputDebugStringA(message.c_str());
	}

	/// -------------------------------------------------------------
	///          UTF-8 文字列からワイド文字列への変換処理
	/// -------------------------------------------------------------
	std::wstring ConvertString(const std::string& str)
	{
		if (str.empty())
		{
			// 空文字は Windows API に渡さず、そのまま空の wstring として返す。
			return std::wstring();
		}

		// 先に必要な文字数を取得してから、確保済みバッファへ変換結果を書き込む。
		auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
		if (sizeNeeded == 0)
		{
			// 変換に失敗した場合は、呼び出し側で扱いやすいよう空文字を返す。
			return std::wstring();
		}

		std::wstring result(sizeNeeded, 0);
		MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
		return result;
	}

	/// -------------------------------------------------------------
	///          ワイド文字列から UTF-8 文字列への変換処理
	/// -------------------------------------------------------------
	std::string ConvertString(const std::wstring& str)
	{
		if (str.empty())
		{
			// 空文字は変換処理を行わず、そのまま空の string として返す。
			return std::string();
		}

		// 先に必要なバイト数を取得してから、確保済みバッファへ変換結果を書き込む。
		auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
		if (sizeNeeded == 0)
		{
			// 変換に失敗した場合は、呼び出し側で扱いやすいよう空文字を返す。
			return std::string();
		}

		std::string result(sizeNeeded, 0);
		WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
		return result;
	}

} // namespace Ken4lowEngine
