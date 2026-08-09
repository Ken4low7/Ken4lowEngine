#pragma once
#include "DX12Include.h"

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///					デバイスの生成クラス
/// -------------------------------------------------------------
class DX12Device
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DirectX 12 デバイスを初期化します。<br/>
	/// 1. CreateDXGIFactory で DXGI ファクトリ(dxgiFactory) を生成<br/>
	/// 2. EnumAdapterByGpuPreference で高パフォーマンス GPU アダプターを列挙・選択(useAdapter)<br/>
	/// 3. D3D_FEATURE_LEVEL 12.2 → 12.1 → 12.0 の順に D3D12CreateDevice を試行し、<br/>
	/// 　　成功した FeatureLevel をログ出力<br/>
	/// 4. 最終的に ID3D12Device(device) を保持<br/>
	/// という流れで、描画に使用する物理デバイスを確定します。<br/>
	/// いずれかのステップで失敗した場合は assert によりアプリを停止します。
	/// </summary>
	void Initialize();

	void Finalize();

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// 生成された ID3D12Device を取得します。
	/// </summary>
	/// <returns>DirectX 12 デバイスへのポインタ。</returns>
	ID3D12Device* GetDevice() const { return device.Get(); }

	/// <summary>
	/// 生成された IDXGIFactory7 を取得します。<br/>
	/// スワップチェイン生成やアダプター列挙などに使用します。
	/// </summary>
	/// <returns>DXGI ファクトリへのポインタ。</returns>
	IDXGIFactory7* GetDXGIFactory() const { return dxgiFactory.Get(); }

private: /// ---------- メンバ変数 ---------- ///

	// DirectX 12 デバイス本体
	ComPtr <ID3D12Device> device;

	// DXGI ファクトリと使用アダプター
	ComPtr <IDXGIFactory7> dxgiFactory;

	// 使用アダプター
	ComPtr <IDXGIAdapter4> useAdapter;
};


} // namespace Ken4lowEngine
