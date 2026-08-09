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
///			デプスステンシルビュー（DSV）を管理するクラス
/// -------------------------------------------------------------
class DSVManager
{
public: /// ---------- メンバ関数 ---------- ///

	/// <summary>
	/// DSVManager のシングルトンインスタンスを取得します。
	/// </summary>
	/// <returns>DSVManager の唯一のインスタンス。</returns>
	static DSVManager* GetInstance();

	/// <summary>
	/// DSV マネージャを初期化します。<br/>
	/// ・DirectXCommon の保持<br/>
	/// ・DSV 用ディスクリプタヒープの生成<br/>
	/// ・DSV デスクリプタサイズの取得<br/>
	/// を行います。
	/// </summary>
	/// <param name="dxCommon">デバイス取得などに使用する DirectXCommon。</param>
	/// <param name="maxDSVCount">このヒープで扱う DSV の最大数。省略時はデフォルト値を使用します。</param>
	void Initialize(DirectXCommon* dxCommon, uint32_t maxDSVCount = kDefaultMaxDSVCount_);

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 深度ステンシルバッファ用のリソースを生成します。<br/>
	/// ・TEXTURE2D / 1 ミップ / D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL<br/>
	/// ・指定フォーマット / サイズ<br/>
	/// でリソースを作成し、初期クリア値（Depth=1.0, Stencil=0）も out 引数で返します。
	/// </summary>
	/// <param name="width">深度バッファの幅。</param>
	/// <param name="height">深度バッファの高さ。</param>
	/// <param name="format">深度ステンシルフォーマット（例：DXGI_FORMAT_D24_UNORM_S8_UINT）。</param>
	/// <param name="outClearValue">作成時に使用したクリア値を返す出力引数。</param>
	/// <returns>生成された深度ステンシルバッファリソース。</returns>
	ComPtr<ID3D12Resource> CreateDepthStencilBuffer(uint32_t width, uint32_t height, DXGI_FORMAT format, D3D12_CLEAR_VALUE& outClearValue);

	ComPtr<ID3D12Resource> CreateShadowMapResource(uint32_t width, uint32_t height);

	/// <summary>
	/// 空いている DSV インデックスを 1 つ確保して返します。<br/>
	/// ・freeIndices_ に解放済みインデックスがあればそれを再利用<br/>
	/// ・なければ useIndex_ から順に新規割り当て<br/>
	/// ・maxDSVCount_ を超えた場合は std::runtime_error を送出<br/>
	/// という挙動になります。スレッドセーフです。
	/// </summary>
	/// <returns>確保された DSV インデックス。</returns>
	uint32_t Allocate();

	/// <summary>
	/// 指定した DSV インデックスを解放します。<br/>
	/// 現在はインデックスの範囲チェックのみ行っています。<br/>
	/// 必要に応じて、freeIndices_ に積んで再利用できるように拡張できます。
	/// </summary>
	/// <param name="dsvIndex">解放する DSV インデックス。</param>
	void Free(uint32_t dsvIndex);

	/// <summary>
	/// 指定された深度バッファリソースに対する DSV を作成します。<br/>
	/// DXGI_FORMAT_D24_UNORM_S8_UINT / TEXTURE2D として DSV を生成します。
	/// </summary>
	/// <param name="dsvIndex">DSV を作成するディスクリプタヒープ上のインデックス。</param>
	/// <param name="depthResource">DSV を作成する対象の深度ステンシルリソース。</param>
	void CreateDSVForDepthBuffer(uint32_t dsvIndex, ID3D12Resource* depthResource);

	/// <summary>
	/// 指定された 2D テクスチャリソースに対する DSV を作成します。<br/>
	/// シャドウマップなどで深度テクスチャを別途扱う場合に使用します。<br/>
	/// フォーマットは DXGI_FORMAT_D24_UNORM_S8_UINT、TEXTURE2D で作成します。
	/// </summary>
	/// <param name="dsvIndex">DSV を作成するディスクリプタヒープ上のインデックス。</param>
	/// <param name="resource">DSV を作成する対象のテクスチャリソース。</param>
	void CreateDSVForTexture2D(uint32_t dsvIndex, ID3D12Resource* resource);

	void CreateDSVForShadowMap(uint32_t dsvIndex, ID3D12Resource* resource);

	/// <summary>Shadow用Texture2DArrayの指定SliceへD32_FLOAT DSVを作成します。</summary>
	void CreateDSVForShadowMapArraySlice(uint32_t dsvIndex, ID3D12Resource* resource, uint32_t arraySlice);

public: /// ---------- ゲッター ---------- ///

	/// <summary>
	/// DSV 用ディスクリプタヒープを取得します。
	/// </summary>
	/// <returns>内部で保持している ID3D12DescriptorHeap。</returns>
	ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

	/// <summary>
	/// 指定インデックスに対応する CPU デスクリプタハンドルを取得します。<br/>
	/// CreateDepthStencilView の第 3 引数として使用します。
	/// </summary>
	/// <param name="index">ディスクリプタヒープ上のインデックス。</param>
	/// <returns>CPU 側の D3D12_CPU_DESCRIPTOR_HANDLE。</returns>
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

private: /// ---------- メンバ変数 ---------- ///

	DirectXCommon* dxCommon_ = nullptr;  // DirectXの共通管理クラス（デバイス取得用）

	uint32_t descriptorSize_ = 0;					  // DSVデスクリプタのサイズ
	static const uint32_t kDefaultMaxDSVCount_ = 32;  // デフォルト最大DSV数
	uint32_t maxDSVCount_ = kDefaultMaxDSVCount_;	  // 最大DSV数
	uint32_t useIndex_ = 0;							  // 次に使用するDSVのインデックス

	std::mutex allocationMutex_;	   // スレッドセーフのためのミューテックス
	std::queue<uint32_t> freeIndices_; // 解放済みのDSVインデックスリスト

	ComPtr<ID3D12DescriptorHeap> descriptorHeap_; // DSV用デスクリプタヒープ

private: /// ---------- コピー禁止 ---------- ///

	/// <summary>
	/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
	/// シングルトンパターンとして使用します。
	/// </summary>
	DSVManager() = default;

	/// <summary>
	/// デフォルトデストラクタ。
	/// </summary>
	~DSVManager() = default;

	/// <summary>
	/// コピーコンストラクタは使用禁止です。
	/// </summary>
	DSVManager(const DSVManager&) = delete;

	/// <summary>
	/// 代入演算子は使用禁止です。
	/// </summary>
	DSVManager& operator=(const DSVManager&) = delete;
};

} // namespace Ken4lowEngine
