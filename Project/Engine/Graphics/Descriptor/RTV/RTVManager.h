#pragma once
#include "DX12Include.h"
#include <mutex>
#include <cstdint>
#include <stdexcept>
#include <queue>

namespace Ken4lowEngine
{


/// ---------- 前方宣言 ---------- ///
class DirectXCommon;


/// -------------------------------------------------------------
///			レンダーターゲットビュー（RTV）を管理するクラス
/// -------------------------------------------------------------
class RTVManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// RTVManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>RTVManager の唯一のインスタンス</returns>
	static RTVManager* GetInstance();

	/// <summary>
	/// RTV マネージャの初期化を行います。<br/>
	/// 内部で RTV 用ディスクリプタヒープを作成し、ディスクリプタサイズを取得します。
	/// </summary>
	/// <param name="dxCommon">デバイス取得などに使用する DirectXCommon</param>
	/// <param name="maxRTVCount">このヒープで扱う RTV の最大数。省略時はデフォルト値を使用します。</param>
	void Initialize(DirectXCommon* dxCommon, uint32_t maxRTVCount = kDefaultMaxRTVCount_);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 空いている RTV インデックスを 1 つ確保します。<br/>
	/// 解放済みのインデックスがあればそれを再利用し、なければ新規に割り当てます。<br/>
	/// 最大数を超えると std::runtime_error を送出します。<br/>
	/// スレッドセーフに動作します。
	/// </summary>
	/// <returns>確保された RTV インデックス</returns>
	uint32_t Allocate();

	/// <summary>
	/// 指定した RTV インデックスを解放し、再利用可能な状態に戻します。<br/>
	/// 範囲外のインデックスが渡された場合は std::runtime_error を送出します。
	/// </summary>
	/// <param name="rtvIndex">解放する RTV インデックス</param>
	void Free(uint32_t rtvIndex);

	/// <summary>
	/// 指定された 2D テクスチャリソースに対する RTV を作成します。<br/>
	/// フォーマットは DXGI_FORMAT_R8G8B8A8_UNORM_SRGB、TEXTURE2D 用 RTV として作成します。
	/// </summary>
	/// <param name="rtvIndex">RTV を作成するディスクリプタヒープ上のインデックス</param>
	/// <param name="resource">RTV を作成する対象のテクスチャリソース</param>
	void CreateRTVForTexture2D(uint32_t rtvIndex, ID3D12Resource* resource);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// RTV 用ディスクリプタヒープを取得します。
	/// </summary>
	/// <returns>内部で保持している ID3D12DescriptorHeap</returns>
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

	/// <summary>
	/// 指定インデックスに対応する CPU デスクリプタハンドルを取得します。<br/>
	/// CreateRenderTargetView の第 3 引数として使用します。
	/// </summary>
	/// <param name="index">ディスクリプタヒープ上のインデックス</param>
	/// <returns>CPU 側の D3D12_CPU_DESCRIPTOR_HANDLE</returns>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;

	uint32_t descriptorSize_ = 0;  // RTVデスクリプタのサイズ
	static const uint32_t kDefaultMaxRTVCount_ = 128; // デフォルトの最大RTV数
	uint32_t maxRTVCount_ = kDefaultMaxRTVCount_; // RTVの最大数
	uint32_t useIndex_ = 0;        // 次に使用するRTVのインデックス

	std::mutex allocationMutex_;  // スレッドセーフのためのミューテックス
	std::queue<uint32_t> freeIndices_;  // 解放済みのRTVインデックスリスト

	ComPtr<ID3D12DescriptorHeap> descriptorHeap_;  // RTV用デスクリプタヒープ

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
	/// シングルトンパターンとして使用します。
	/// </summary>
	RTVManager() = default;

	/// <summary>
	/// デフォルトデストラクタ
	/// </summary>
	~RTVManager() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	RTVManager(const RTVManager&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	RTVManager& operator=(const RTVManager&) = delete;
};


} // namespace Ken4lowEngine
