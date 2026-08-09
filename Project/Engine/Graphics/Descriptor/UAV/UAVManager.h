#pragma once
#include "DX12Include.h"
#include <memory>
#include <mutex>
#include <queue>
#include <cstdint>
#include <stdexcept>

namespace Ken4lowEngine
{

/// ---------- 前方宣言 ---------- ///
class DirectXCommon;

/// -------------------------------------------------------------
///				　		 UAVマネージャークラス
/// -------------------------------------------------------------
class UAVManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// UAVManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>UAVManager の唯一のインスタンスへのポインタ</returns>
	static UAVManager* GetInstance();

	/// <summary>
	/// UAV 用ディスクリプタヒープの初期化を行います。
	/// 内部で CBV_SRV_UAV タイプのディスクリプタヒープを kMaxUAVCount 分確保し、
	/// デスクリプタサイズを取得します。
	/// </summary>
	/// <param name="dxCommon">デバイスやコマンドリスト取得に使用する DirectXCommon</param>
	void Initialize(DirectXCommon* dxCommon);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// Dispatch 前に、このマネージャが保持しているディスクリプタヒープを
	/// コマンドリストへセットします。
	/// Compute Shader から UAV/SRV にアクセスする前に呼び出してください。
	/// </summary>
	void PreDispatch();

	/// <summary>
	/// Texture2D 用の UAV を作成します。
	/// 事前に対象テクスチャは UAV 対応で生成されている必要があります。
	/// </summary>
	/// <param name="uavIndex">UAV を作成するヒープ上のインデックス（0 ～ kMaxUAVCount-1）</param>
	/// <param name="pResource">UAV を作成する対象の ID3D12Resource（Texture2D）</param>
	/// <param name="Format">テクスチャのフォーマット</param>
	/// <param name="MipLevels">UAV 対象とするミップレベル</param>
	void CreateUAVForTexture2D(uint32_t uavIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	/// <summary>
	/// この UAV ヒープ（CBV_SRV_UAV 型）の上に Texture2D 用の SRV を作成します。
	/// UAV と同じヒープに SRV もまとめて配置したい場合に使用します。
	/// </summary>
	/// <param name="srvIndex">SRV を作成するヒープ上のインデックス</param>
	/// <param name="pResource">SRV を作成する対象の ID3D12Resource（Texture2D）</param>
	/// <param name="Format">テクスチャのフォーマット</param>
	/// <param name="MipLevels">使用するミップレベル数</param>
	void CreateSRVForTexture2DOnThisHeap(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

	/// <summary>
	/// Raw バッファ用の UAV を作成します。
	/// DXGI_FORMAT_R32_TYPELESS + D3D12_BUFFER_UAV_FLAG_RAW として設定されます。
	/// </summary>
	/// <param name="uavIndex">UAV を作成するヒープ上のインデックス</param>
	/// <param name="pResource">UAV を作成する対象のバッファリソース</param>
	/// <param name="bufferSize">バッファ全体のバイトサイズ。4 バイト単位で要素数に変換されます。</param>
	void CreateUAVForBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT64 bufferSize);

	/// <summary>
	/// Structured Buffer 用の UAV を作成します。
	/// DXGI_FORMAT_UNKNOWN + StructureByteStride を指定して構造化バッファとして扱います。
	/// </summary>
	/// <param name="uavIndex">UAV を作成するヒープ上のインデックス</param>
	/// <param name="pResource">UAV を作成する対象のバッファリソース</param>
	/// <param name="numElements">バッファ内の要素数</param>
	/// <param name="structureByteStride">1 要素あたりのバイトサイズ</param>
	void CreateUAVForStructuredBuffer(uint32_t uavIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

	/// <summary>
	/// Structured Buffer 用の SRV を作成します。
	/// UAV と同じディスクリプタヒープ上に SRV を作成したい場合に使用します。
	/// </summary>
	/// <param name="srvIndex">SRV を作成するヒープ上のインデックス</param>
	/// <param name="pResource">SRV を作成する対象のバッファリソース</param>
	/// <param name="numElements">バッファ内の要素数</param>
	/// <param name="structureByteStride">1 要素あたりのバイトサイズ</param>
	void CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

public: /// ---------- 容量の確保 ---------- ///

	/// <summary>
	/// ディスクリプタヒープ上の空きインデックスを 1 つ確保して返します。
	/// 内部で空きインデックスの再利用と useIndex_ のインクリメントを行い、
	/// スレッドセーフに動作します。
	/// 確保可能な最大数を超えた場合は例外を送出します。
	/// </summary>
	/// <returns>確保されたディスクリプタインデックス</returns>
	uint32_t Allocate();

	/// <summary>
	/// 指定したインデックスを解放し、再利用可能な状態に戻します。
	/// 解放されたインデックスは内部キューに蓄積され、次回の Allocate で再利用されます。
	/// 範囲外のインデックスが指定された場合は例外を送出します。
	/// </summary>
	/// <param name="srvIndex">解放するディスクリプタインデックス</param>
	void Free(uint32_t srvIndex);

	/// <summary>
	/// 指定インデックスに対応する CPU デスクリプタハンドルを取得します。
	/// CreateUAV / CreateSRV 時の「書き込み先ハンドル」取得に使用します。
	/// </summary>
	/// <param name="index">ディスクリプタヒープ上のインデックス</param>
	/// <returns>CPU 側の D3D12_CPU_DESCRIPTOR_HANDLE</returns>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

	/// <summary>
	/// 指定インデックスに対応する GPU デスクリプタハンドルを取得します。
	/// ルートディスクリプタテーブルなどに設定してシェーダから参照する際に使用します。
	/// </summary>
	/// <param name="index">ディスクリプタヒープ上のインデックス</param>
	/// <returns>GPU 側の D3D12_GPU_DESCRIPTOR_HANDLE</returns>
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr; // DirectXCommonのインスタンス

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc_; // デスクリプタヒープの設定
	ComPtr<ID3D12DescriptorHeap> descriptorHeap_; // UAV用のデスクリプタヒープ
	UINT descriptorSize_ = 0; // デスクリプタサイズ
	static constexpr uint32_t kMaxUAVCount = 512; // 最大UAV数

	// 次に使用するSRVインデックス
	uint32_t useIndex_ = 0;

	// スレッドセーフ用
	std::mutex allocationMutex_;

	// 空きインデックスのリスト
	std::queue<uint32_t> freeIndices_;

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。
	/// シングルトンとして使用します。
	/// </summary>
	UAVManager() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~UAVManager() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	UAVManager(const UAVManager&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	const UAVManager& operator=(const UAVManager&) = delete;
};


} // namespace Ken4lowEngine
