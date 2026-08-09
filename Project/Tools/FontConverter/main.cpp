#include <Windows.h>
#include <cstdint>
#include <cassert>

#include "FontConverter.h"

/// <summary>
/// FontConverter.exe に渡されるコマンドライン引数の位置を表す列挙型
/// argv[0] は実行ファイル自身のパス、argv[1] は変換対象のフォントファイルパスとして扱う。
/// </summary>
enum class CommandLineArgument : std::uint8_t
{
	kApplicationPath, // 実行ファイルのパス
	kFilePath,		  // フォントファイルのパス

	NumArguments	  // 必要な引数の数
};

/// <summary>
/// FontConverter のエントリーポイント
/// コマンドライン引数の確認、COM初期化、FontConverter本体の実行、COM終了処理を順番に行う。
/// </summary>
int wmain(int argc, wchar_t* argv[])
{
	// コマンドライン引数の数をチェック
	if (argc < static_cast<int>(CommandLineArgument::NumArguments))
	{
		FontConverter::OutputUsage(); // コマンドライン引数の使い方を出力
		return 1;
	}

	// DirectWrite / WIC などのCOMベースAPIを使うため、変換処理の前にCOMを初期化する。
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr) && "COMライブラリの初期化に失敗しました。");

	// 変換処理の本体を担当する FontConverter を生成する。
	FontConverter fontConverter;

	// argv[2]以降は -size や -charsetFile などのオプションとして FontConverter に渡す。
	int numOptions = argc - static_cast<int>(CommandLineArgument::NumArguments);
	wchar_t** options = argv + static_cast<int>(CommandLineArgument::NumArguments);

	// argv[1] のフォントファイルを入力として、アトラス画像とメタデータJSONを出力する。
	const bool success = fontConverter.ConvertFont(argv[static_cast<std::uint8_t>(CommandLineArgument::kFilePath)], numOptions, options);

	// COMを使い終わったので、プロセス終了前に後始末する。
	CoUninitialize();
	return success ? 0 : 1;
}