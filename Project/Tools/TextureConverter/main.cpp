#include <cstdint>
#include <iostream>
#include <cassert>

#include "TextureConverter.h"

/// ---------- コマンドライン引数 ---------- ///
enum class CommandLineArgument : std::uint8_t
{
	kApplicationPath, // アプリケーションのパス
	kFilePath,        // ファイルのパス

	NumArguments	  // 引数の数
};


// コマンドライン引数を表示するプログラム
int main(int argc, char* argv[])
{
	// コマンドライン引数の数をチェック
	if (argc < static_cast<int>(CommandLineArgument::NumArguments))
	{
		// 使用方法を出力
		TextureConverter::OoutputUsage();
		return 0;
	}

	// COMライブラリの初期化
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(SUCCEEDED(hr) && "COMライブラリの初期化に失敗しました。");

	// TextureConveterのインスタンスを作成
	TextureConverter textureConveter;

	int numOptions = argc - static_cast<int>(CommandLineArgument::NumArguments);

	// オプションの配列を作成 (ダブルポインタ)
	char** options = argv + static_cast<int>(CommandLineArgument::NumArguments);

	// テクスチャ変換
	textureConveter.ConvertTextureWICToDDS(argv[static_cast<uint8_t>(CommandLineArgument::kFilePath)], numOptions, options);

	// COMライブラリの終了処理
	CoUninitialize();

	//// プログラムの終了前に一時停止
	//system("pause");

	// 正常終了
	return 0;
}