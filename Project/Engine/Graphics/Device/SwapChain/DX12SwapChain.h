#pragma once
#include <Windows.h>

#include <cstdint>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class WinApp;

/// -------------------------------------------------------------
///				スワップチェインの生成クラス
/// -------------------------------------------------------------
class DX12SwapChain
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// スワップチェインを生成・初期化します。<br/>
	/// ・画面サイズ、フォーマット、バッファ数(ダブルバッファ)などを設定した DXGI_SWAP_CHAIN_DESC1 を構築し、<br/>
	/// ・IDXGIFactory7::CreateSwapChainForHwnd を用いて IDXGISwapChain4 を作成<br/>
	/// ・各バックバッファ(ID3D12Resource) を GetBuffer() で取得<br/>
	/// という流れで初期化を行います。<br/>
	/// 生成に失敗した場合は assert で停止します。
	/// </summary>
	/// <param name="winApp">ウィンドウハンドル(HWND)取得に使用する WinApp。</param>
	/// <param name="dxgiFactory">スワップチェイン生成に使用する DXGI ファクトリ。</param>
	/// <param name="commandQueue">スワップチェインに紐づけるコマンドキュー。</param>
	/// <param name="Width">バックバッファの幅（クライアント領域の幅）。</param>
	/// <param name="Height">バックバッファの高さ（クライアント領域の高さ）。</param>
	void Initialize(WinApp* winApp, IDXGIFactory7* dxgiFactory, ID3D12CommandQueue* commandQueue, uint32_t Width, uint32_t Height);

	void Finalize()
	{
		swapChain.Reset();
		for (auto& resource : swapChainResources)
		{
			resource.Reset();
		}
	}

	void Resize(uint32_t width, uint32_t height);

private:
	/// バックバッファ名と初期ステートを揃えてDebugLayerで特定しやすくする。
	void CacheBackBuffer(uint32_t index);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// スワップチェイン本体(IDXGISwapChain4)を取得します。<br/>
	/// Present や CurrentBackBufferIndex の取得などに使用します。
	/// </summary>
	/// <returns>内部で保持している IDXGISwapChain4。</returns>
	IDXGISwapChain4* GetSwapChain() const { return swapChain.Get(); }

	/// <summary>
	/// 指定インデックスのバックバッファリソースを取得します。<br/>
	/// インデックスは 0 ～ BufferCount-1 の範囲で指定します。
	/// </summary>
	/// <param name="num">取得したいバックバッファのインデックス。</param>
	/// <returns>指定されたバックバッファの ID3D12Resource。</returns>
	ID3D12Resource* GetSwapChainResources(uint32_t num) const { return swapChainResources[num].Get(); }

	/// <summary>
	/// スワップチェイン設定(DXGI_SWAP_CHAIN_DESC1)への参照を取得します。<br/>
	/// バッファ数 / フォーマット / 使用用途などの情報にアクセスする際に使用します。
	/// </summary>
	/// <returns>内部で保持している DXGI_SWAP_CHAIN_DESC1 への参照。</returns>
	DXGI_SWAP_CHAIN_DESC1& GetSwapChainDesc() { return swapChainDesc; }

	/// <summary>
	/// 指定インデックスのバックバッファの現在のリソースステートを取得します。<br/>
	/// バリア発行の省略判断などに使用できます。
	/// </summary>
	/// <param name="index">バックバッファインデックス。</param>
	/// <returns>記録している D3D12_RESOURCE_STATES。</returns>
	D3D12_RESOURCE_STATES GetBackBufferState(uint32_t index) { return backBufferStates[index]; }

public: /// ---------- セッター ---------- ///

	/// <summary>
	/// 指定インデックスのバックバッファのリソースステートを記録します。<br/>
	/// ResourceBarrier を発行したあとに、この関数で最新ステートに更新しておく想定です。
	/// </summary>
	/// <param name="index">バックバッファインデックス。</param>
	/// <param name="state">新しいリソースステート。</param>
	void SetBackBufferState(uint32_t index, D3D12_RESOURCE_STATES state) { backBufferStates[index] = state; }

private: /// ---------- メンバ変数 ---------- ///

	// スワップチェイン本体
	Microsoft::WRL::ComPtr <IDXGISwapChain4> swapChain;

	// バックバッファリソース（ダブルバッファ）
	Microsoft::WRL::ComPtr <ID3D12Resource> swapChainResources[2];

	// スワップチェイン設定情報
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	// 各バックバッファのリソースステート管理用
	D3D12_RESOURCE_STATES backBufferStates[2] = {};
};


} // namespace Ken4lowEngine
