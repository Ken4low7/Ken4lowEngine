#include "D3DResourceLeakChecker.h"
#include <dxgidebug.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace Ken4lowEngine
{

#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

bool D3DResourceLeakChecker::hasReported_ = false;

/// -------------------------------------------------------------
///			 				デストラクタ
/// -------------------------------------------------------------
D3DResourceLeakChecker::~D3DResourceLeakChecker()
{
	if (!hasReported_)
	{
		ReportLiveObjects();
	}
}

void D3DResourceLeakChecker::ReportLiveObjects()
{
	// DirectXCommon破棄前に呼べるよう、ライブオブジェクト報告処理を明示関数へ分離する。
	Microsoft::WRL::ComPtr<IDXGIDebug> debug;

	// デバッグインターフェースの取得
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		//ライブオブジェクトの報告
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);   // 全てのライブオブジェクトを報告
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);   // アプリケーションのライブオブジェクトを報告
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL); // D3D12のライブオブジェクトを報告
		hasReported_ = true;
	}
}

} // namespace Ken4lowEngine
