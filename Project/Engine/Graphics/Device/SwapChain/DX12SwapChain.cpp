#include "DX12SwapChain.h"
#include "WinApp.h"

#include <cassert>
#include <cwchar>

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///				スワップチェインの初期化処理
/// -------------------------------------------------------------
void DX12SwapChain::Initialize(WinApp* winApp, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, uint32_t Width, uint32_t Height)
{
	HRESULT hr{};

	swapChain = nullptr;

	//スワップチェーンを生成する
	swapChainDesc.Width = Width;									// 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = Height;									// 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				// 色の形式
	swapChainDesc.SampleDesc.Count = 1;								// マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	// 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2;									// ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;		// モニタにうつしたら、中身を破壊

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, winApp->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain.GetAddressOf()));
	assert(SUCCEEDED(hr));

	// SwapChainからResourceを引っ張り、BackBuffer名と状態を一元管理する
	CacheBackBuffer(0);
	CacheBackBuffer(1);
}

void DX12SwapChain::Resize(uint32_t width, uint32_t height)
{
	if (!swapChain) return;

	// 既存参照を外す（重要）
	for (auto& r : swapChainResources) r.Reset();

	// 0 / UNKNOWN で「現状維持」
	HRESULT hr = swapChain->ResizeBuffers(
		0, width, height,
		DXGI_FORMAT_UNKNOWN,
		0
	);
	assert(SUCCEEDED(hr));

	// 再取得時もBackBuffer0/1の名前を付け直してUnnamed Resourceを避ける
	CacheBackBuffer(0);
	CacheBackBuffer(1);

	// descも更新しておくと便利
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
}


void DX12SwapChain::CacheBackBuffer(uint32_t index)
{
	HRESULT hr = swapChain->GetBuffer(index, IID_PPV_ARGS(&swapChainResources[index]));
	assert(SUCCEEDED(hr));

	wchar_t name[32]{};
	swprintf_s(name, L"BackBuffer%u", index);
	// DebugLayerのResource名にBackBuffer番号を出し、RTV状態違反の対象を特定しやすくする。
	swapChainResources[index]->SetName(name);

	// SwapChainの取得直後はPRESENT(COMMON)として扱い、DirectXCommonのBarrier beforeと一致させる。
	backBufferStates[index] = D3D12_RESOURCE_STATE_PRESENT;
}

} // namespace Ken4lowEngine
