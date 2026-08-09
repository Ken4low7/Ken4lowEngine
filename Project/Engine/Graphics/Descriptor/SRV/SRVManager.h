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
	///			シェーダーリソースビュー（SRV）を管理するクラス
	/// -------------------------------------------------------------
	class SRVManager
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SRVManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>SRVManager の唯一のインスタンス</returns>
		static SRVManager* GetInstance();

		/// <summary>
		/// SRV 用ディスクリプタヒープを初期化します。<br/>
		/// CBV_SRV_UAV 型で kMaxSRVCount 個分のディスクリプタを確保し、<br/>
		/// シェーダーから参照できるよう SHADER_VISIBLE フラグを立てます。
		/// </summary>
		/// <param name="dxCommon">デバイスやコマンドリスト取得に使用する DirectXCommon</param>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// SRV 用ディスクリプタヒープの終了処理を行います。
		/// 内部で保持しているディスクリプタヒープを解放します。
		/// </summary>
		void Finalize();

		/// <summary>
		/// Texture2D 用の SRV を作成します。<br/>
		/// 通常のカラー / テクスチャマップ用の SRV 生成に使用します。
		/// </summary>
		/// <param name="srvIndex">SRV を作成するディスクリプタヒープ上のインデックス。</param>
		/// <param name="pResource">SRV を作成する対象のテクスチャリソース</param>
		/// <param name="Format">テクスチャのフォーマット</param>
		/// <param name="MipLevels">使用するミップレベル数</param>
		void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

		/// <summary>
		/// Structured Buffer 用の SRV を作成します。<br/>
		/// DXGI_FORMAT_UNKNOWN + StructureByteStride を使って構造化バッファとして扱います。
		/// </summary>
		/// <param name="srvIndex">SRV を作成するディスクリプタヒープ上のインデックス。</param>
		/// <param name="pResource">SRV を作成する対象のバッファリソース</param>
		/// <param name="numElements">バッファ内の要素数</param>
		/// <param name="structureByteStride">1 要素あたりのサイズ（バイト）</param>
		void CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

		void CreateSRVForShadowMap(uint32_t srvIndex, ID3D12Resource* shadowMap);

		/// <summary>CSM用の深度Texture2DArray SRVを作成します。</summary>
		void CreateSRVForShadowMapArray(uint32_t srvIndex, ID3D12Resource* shadowMapArray, uint32_t arraySize);

		/// <summary>Point Light Cube Shadow用の深度TextureCube SRVを作成します。</summary>
		void CreateSRVForShadowCube(uint32_t srvIndex, ID3D12Resource* shadowCube);

		/// <summary>
		/// 描画前に、この SRV ヒープをコマンドリストへセットします。<br/>
		/// SRV を使う前に必ず呼び出してください。
		/// </summary>
		void PreDraw();

		/// <summary>
		/// 指定したルートパラメータに、SRV テーブルをセットします。<br/>
		/// 内部で GetGPUDescriptorHandle(srvIndex) を呼び出し、<br/>
		/// SetGraphicsRootDescriptorTable を発行します。
		/// </summary>
		/// <param name="RootParameterIndex">ルートシグネチャ上のパラメータインデックス</param>
		/// <param name="srvIndex">ディスクリプタヒープ上の SRV インデックス</param>
		void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

		/// <summary>
		/// 深度バッファを Shader Resource として参照するための SRV を作成します。<br/>
		/// シャドウマップやポストプロセスなどで深度値を読みたいときに使用します。
		/// </summary>
		/// <param name="srvIndex">SRV を作成するディスクリプタインデックス</param>
		/// <param name="depthBuffer">SRV を作成する対象の深度バッファリソース</param>
		void CreateSRVForDepthBuffer(uint32_t srvIndex, ID3D12Resource* depthBuffer);

		/// <summary>
		/// ディスクリプタヒープ上の空きインデックスを 1 つ確保して返します。<br/>
		/// freeIndices に空きがあればそれを再利用し、なければ useIndex から新規に割り当てます。<br/>
		/// kMaxSRVCount を超えた場合は例外を送出します。<br/>
		/// スレッドセーフに動作します。
		/// </summary>
		/// <returns>確保された SRV インデックス</returns>
		uint32_t Allocate();

		/// <summary>
		/// 指定したインデックスを解放し、再利用可能な状態に戻します。<br/>
		/// 解放されたインデックスは freeIndices に積まれ、次回 Allocate で使用されます。
		/// </summary>
		/// <param name="srvIndex">解放する SRV インデックス</param>
		void Free(uint32_t srvIndex);

	public: /// ---------- ゲッター ---------- ///

		/// <summary>
		/// SRV 用ディスクリプタヒープを取得します。
		/// </summary>
		/// <returns>内部で保持している ID3D12DescriptorHeap ポインタ</returns>
		ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

		/// <summary>
		/// 任意の用途向けにディスクリプタヒープを生成するユーティリティ関数です。<br/>
		/// heapType と numDescriptors、シェーダー可視フラグを指定してヒープを作成します。
		/// </summary>
		/// <param name="device">ヒープを作成するデバイス</param>
		/// <param name="heapType">ディスクリプタヒープの種類</param>
		/// <param name="numDescriptors">確保するディスクリプタ数</param>
		/// <param name="shadervisible">シェーダーから参照可能にするかどうか</param>
		/// <returns>作成されたディスクリプタヒープ。</returns>
		ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

		/// <summary>
		/// 指定インデックスに対応する CPU デスクリプタハンドルを取得します。<br/>
		/// CreateShaderResourceView の第 3 引数として使用します。
		/// </summary>
		/// <param name="index">ディスクリプタヒープ上のインデックス</param>
		/// <returns>CPU 側の D3D12_CPU_DESCRIPTOR_HANDLE。</returns>
		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);

		/// <summary>
		/// 指定インデックスに対応する GPU デスクリプタハンドルを取得します。<br/>
		/// SetGraphicsRootDescriptorTable でシェーダーから SRV にアクセスさせる際に使用します。
		/// </summary>
		/// <param name="index">ディスクリプタヒープ上のインデックス</param>
		/// <returns>GPU 側の D3D12_GPU_DESCRIPTOR_HANDLE</returns>
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

		/// <summary>
		/// SRV 用ディスクリプタ 1 つあたりのサイズ（バイト）を取得します。
		/// </summary>
		uint32_t GetDescriptorSize() const { return descriptorSize; }

		/// <summary>
		/// このマネージャが扱う SRV の最大数を取得します。
		/// </summary>
		uint32_t GetkMaxSRVCount() const { return kMaxSRVCount; }

	private: /// ---------- メンバ変数 ---------- ///

		// DirectXCommonのポインタ
		DirectXCommon* dxCommon_ = nullptr;

		// 最大SRV数（最大テクスチャ枚数）
		static const uint32_t kMaxSRVCount = 1024;

		// SRV用のデスクリプタサイズ
		uint32_t descriptorSize = 0;

		// SRV用デスクリプタヒープ
		ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

		// 次に使用するSRVインデックス
		uint32_t useIndex = 1;

		// スレッドセーフ用
		std::mutex allocationMutex;

		// 空きインデックスのリスト
		std::queue<uint32_t> freeIndices;

	private: /// ---------- コピー禁止 ---------- ///

		/// <summary>
		/// 外部からの生成を禁止するためのプライベートコンストラクタ。<br/>
		/// シングルトンパターンとして利用します。
		/// </summary>
		SRVManager() = default;

		/// <summary>
		/// デフォルトデストラクタ
		/// </summary>
		~SRVManager() = default;

		/// <summary>
		/// コピーコンストラクタは使用禁止です。
		/// </summary>
		SRVManager(const SRVManager&) = delete;

		/// <summary>
		/// 代入演算子は使用禁止です。
		/// </summary>
		SRVManager& operator=(const SRVManager&) = delete;
	};


} // namespace Ken4lowEngine
