#include "GameApplication.h"
#include "D3DResourceLeakChecker.h"
#include "Engine/Core/Diagnostics/CrashReporter.h"

#include <exception>

using namespace Ken4lowEngine;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// Phase 12ではEngine初期化より先にCrash Reporterを登録し、起動途中の例外も回収する。
	CrashReporter::Install("CrashReports");

#ifdef _DEBUG
	// シングルトン群の破棄後に最終報告するため、リークチェッカーを先に生成したstaticへ変更する。
	static D3DResourceLeakChecker resourceLeakCheck;
#endif // _DEBUG

	try
	{
		// Frameworkの派生クラスであるGameEngineを使用
		std::unique_ptr<Framework> game = std::make_unique<GameApplication>();

		// 実行処理
		game->Run();
	}
	catch (const std::exception& exception)
	{
		// Crash report書き込み失敗時も元の例外終了を優先するため、ここでは戻り値を明示的に破棄する。
		(void)CrashReporter::WriteManualReport(exception.what());
		CrashReporter::Uninstall();
		return -1;
	}
	catch (...)
	{
		(void)CrashReporter::WriteManualReport("Unknown C++ exception escaped WinMain.");
		CrashReporter::Uninstall();
		return -1;
	}

	CrashReporter::Uninstall();
	return 0;
}
