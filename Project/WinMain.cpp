#include "GameApplication.h"
#include "D3DResourceLeakChecker.h"

using namespace Ken4lowEngine;

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#ifdef _DEBUG
	// シングルトン群の破棄後に最終報告するため、リークチェッカーを先に生成したstaticへ変更する。
	static D3DResourceLeakChecker resourceLeakCheck;
#endif // _DEBUG

	// Frameworkの派生クラスであるGameEngineを使用
	std::unique_ptr<Framework> game = std::make_unique<GameApplication>();

	// 実行処理
	game->Run();

	return 0;
}
