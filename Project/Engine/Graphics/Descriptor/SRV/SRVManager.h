#pragma once
#include "DX12Include.h"
#include <mutex>
#include <cstdint>
#include <stdexcept>
#include <queue>
#include <vector>

namespace Ken4lowEngine
{


	/// ---------- 前方宣言 ---------- ///
	class DirectXCommon;


	/// -------------------------------------------------------------
	///			シェーダーリソースビュー（SRV）を管理するクラス
	/// -------------------------------------------------------------
	class SRVManager
	{
	public: /// ---------- 型定義 ---------- ///

		struct TransientDescriptorAllocation
		{
			uint32_t firstIndex = UINT32_MAX;
			uint32_t count = 0;
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
			UINT64 retireFenceValue = 0;

			[[nodiscard]] bool IsValid() const { return firstIndex != UINT32_MAX && count > 0; }
		};

		struct DescriptorStats
		{
			uint32_t persistentCapacity = 0;
			uint32_t persistentInUse = 0;
			uint32_t persistentHighWater = 0;
			uint32_t transientCapacity = 0;
			uint32_t transientInFlight = 0;
			uint32_t transientHighWater = 0;
			uint32_t pendingTransientRangeCount = 0;
			uint64_t transientAllocationCount = 0;
			uint64_t transientReclaimedCount = 0;
			uint64_t exhaustionCount = 0;
		};

	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>
		/// SRVManager のシングルトンインスタンスを取得します。
		/// </summary>
		/// <returns>SRVManager の唯一のインスタンス</returns>
		static SRVManager* GetInstance();

		/// <summary>
		/// SRV 用ディスクリプタヒープを初期化します。<br/>
		/// Persistent領域とFence安全なTransient領域を同一Shader Visible Heap内で分離します。
		/// </summary>
		/// <param name="dxCommon">デバイスやコマンドリスト取得に使用する DirectXCommon</param>
		void Initialize(DirectXCommon* dxCommon);

		/// <summary>
		/// SRV 用ディスクリプタヒープの終了処理を行います。
		/// </summary>
		void Finalize();

		/// <summary>
		/// Texture2D 用の SRV を作成します。
		/// </summary>
		void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);

		/// <summary>
		/// Structured Buffer 用の SRV を作成します。
		/// </summary>
		void CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);

		void CreateSRVForShadowMap(uint32_t srvIndex, ID3D12Resource* shadowMap);

		/// <summary>CSM用の深度Texture2DArray SRVを作成します。</summary>
		void CreateSRVForShadowMapArray(uint32_t srvIndex, ID3D12Resource* shadowMapArray, uint32_t arraySize);

		/// <summary>Point Light Cube Shadow用の深度TextureCube SRVを作成します。</summary>
		void CreateSRVForShadowCube(uint32_t srvIndex, ID3D12Resource* shadowCube);

		/// <summary>
		/// 描画前に、この SRV ヒープをコマンドリストへセットします。
		/// </summary>
		void PreDraw();

		/// <summary>
		/// 指定したルートパラメータに、SRV テーブルをセットします。
		/// </summary>
		void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);

		/// <summary>
		/// 深度バッファを Shader Resource として参照するための SRV を作成します。
		/// </summary>
		void CreateSRVForDepthBuffer(uint32_t srvIndex, ID3D12Resource* depthBuffer);

		/// <summary>
		/// Persistent用途のディスクリプタを1つ確保します。Texture / ImGui / 長寿命Bufferはこの経路を使用します。
		/// </summary>
		uint32_t Allocate();

		/// <summary>
		/// Persistent用途のディスクリプタを解放します。GPU参照中のAssetはGpuDeferredReleaseQueue経由で呼び出してください。
		/// </summary>
		void Free(uint32_t srvIndex);

		/// <summary>
		/// RenderGraph / Pass向けに連続した一時ディスクリプタを確保します。
		/// 現在記録中CommandListに対応する次回Fence完了までは自動的に再利用されません。
		/// </summary>
		TransientDescriptorAllocation AllocateTransient(uint32_t count = 1);

		/// <summary>完了Fenceまで到達したTransient Descriptor Rangeを再利用可能領域へ戻します。</summary>
		void CollectTransient();

	public: /// ---------- ゲッター ---------- ///

		ID3D12DescriptorHeap* GetDescriptorHeap() const { return descriptorHeap_.Get(); }

		ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shadervisible);

		D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
		D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

		uint32_t GetDescriptorSize() const { return descriptorSize; }
		uint32_t GetkMaxSRVCount() const { return kMaxSRVCount; }
		uint32_t GetPersistentSRVCapacity() const { return kTransientBeginIndex - 1; }
		uint32_t GetTransientSRVCapacity() const { return kTransientSRVCount; }
		uint32_t GetTransientSRVBeginIndex() const { return kTransientBeginIndex; }
		DescriptorStats GetDescriptorStats() const;

	private: /// ---------- 内部型 ---------- ///

		struct DescriptorRange
		{
			uint32_t firstIndex = 0;
			uint32_t count = 0;
		};

		struct PendingTransientRange
		{
			DescriptorRange range{};
			UINT64 retireFenceValue = 0;
		};

		void CollectTransientLocked();
		void InsertFreeTransientRangeLocked(DescriptorRange range);

	private: /// ---------- メンバ変数 ---------- ///

		DirectXCommon* dxCommon_ = nullptr;

		// Phase 9.5ではHeap末尾をFrame/Pass専用に予約し、Persistent Descriptorとの誤再利用を構造的に防ぐ。
		static constexpr uint32_t kMaxSRVCount = 4096;
		static constexpr uint32_t kTransientSRVCount = 1024;
		static constexpr uint32_t kTransientBeginIndex = kMaxSRVCount - kTransientSRVCount;

		uint32_t descriptorSize = 0;
		ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

		uint32_t useIndex = 1;
		mutable std::mutex allocationMutex;
		std::queue<uint32_t> freeIndices;
		std::vector<uint8_t> persistentAllocated_;
		std::vector<DescriptorRange> freeTransientRanges_;
		std::vector<PendingTransientRange> pendingTransientRanges_;
		DescriptorStats descriptorStats_{};

	private: /// ---------- コピー禁止 ---------- ///

		SRVManager() = default;
		~SRVManager() = default;
		SRVManager(const SRVManager&) = delete;
		SRVManager& operator=(const SRVManager&) = delete;
	};


} // namespace Ken4lowEngine
