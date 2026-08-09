#pragma once
#include "DX12Include.h"

namespace Ken4lowEngine
{


/// -------------------------------------------------------------
///						リソース管理クラス
/// -------------------------------------------------------------
class ResourceManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// アップロードヒープ上に配置されたバッファリソースを作成します。<br/>
	/// 小さな定数バッファや、CPU から頻繁に書き込む頂点バッファなどに利用する
	/// 便利関数です。<br/>
	/// 内部的には以下の設定で CreateCommittedResource を呼び出します：<br/>
	/// ・D3D12_HEAP_TYPE_UPLOAD<br/>
	/// ・D3D12_RESOURCE_FLAG_NONE<br/>
	/// ・D3D12_RESOURCE_STATE_GENERIC_READ
	/// </summary>
	/// <param name="device">リソースを作成する ID3D12Device</param>
	/// <param name="size">バッファのサイズ（バイト単位）</param>
	static ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* device, size_t size);

	/// <summary>
	/// バッファリソースの作成を行う汎用関数です。<br/>
	/// ヒープ種別・リソースフラグ・初期ステートを呼び出し側で指定できます。<br/>
	/// リソースディスクリプタはバッファ用（D3D12_RESOURCE_DIMENSION_BUFFER / ROW_MAJOR）として設定されます。
	/// </summary>
	/// <param name="device">リソースを作成する ID3D12Device</param>
	/// <param name="size">バッファのサイズ（バイト単位）</param>
	/// <param name="type">ヒープ種別（UPLOAD / DEFAULT / READBACK など）</param>
	/// <param name="flags">バッファに付与するリソースフラグ。</param>
	/// <param name="initState">作成時のリソースステート</param>
	/// <returns>作成されたバッファリソース</returns>
	static ComPtr<ID3D12Resource> CreateBufferResource(
		ID3D12Device* device,
		UINT64 size,
		D3D12_HEAP_TYPE type,
		D3D12_RESOURCE_FLAGS flags,
		D3D12_RESOURCE_STATES initState);
};


} // namespace Ken4lowEngine
