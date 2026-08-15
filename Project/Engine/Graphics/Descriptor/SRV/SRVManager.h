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
			uint32_t frameIndex = UINT32_MAX;
			D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
			D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};

			[[nodiscard]] bool IsValid() const { return firstIndex != UINT32_MAX && count > 0; }
		};

		struct DescriptorStats
		{
			uint32_t persistentCapacity = 0;
			uint32_t persistentInUse = 0;
			uint32_t persistentHighWater = 0;
			uint32_t transientCapacity = 0;
			uint32_t transientCapacityPerFrame = 0;
			uint32_t transientInUse = 0;
			uint32_t transientHighWater = 0;
			uint32_t currentFrameIndex = 0;
			uint64_t transientAllocationCount = 0;
			uint64_t transientReclaimedCount = 0;
			uint64_t transientFrameRecycleCount = 0;
			uint64_t exhaustionCount = 0;
		};

	public: /// ---------- メンバ関数 ---------- ///

		static SRVManager* GetInstance();

		/// <summary>
		/// SRV 用ディスクリプタヒープを初期化し、Persistent領域とFrame Transient領域を分離します。
		/// </summary>
		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		void CreateSRVForTexture2D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
		void CreateSRVForTexture3D(uint32_t srvIndex, ID3D12Resource* pResource, DXGI_FORMAT Format, UINT MipLevels);
		void CreateSRVForStructureBuffer(uint32_t srvIndex, ID3D12Resource* pResource, UINT numElements, UINT structureByteStride);
		void CreateSRVForShadowMap(uint32_t srvIndex, ID3D12Resource* shadowMap);
		void CreateSRVForShadowMapArray(uint32_t srvIndex, ID3D12Resource* shadowMapArray, uint32_t arraySize);
		void CreateSRVForShadowCube(uint32_t srvIndex, ID3D12Resource* shadowCube);
		void PreDraw();
		void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, uint32_t srvIndex);
		void CreateSRVForDepthBuffer(uint32_t srvIndex, ID3D12Resource* depthBuffer);

		/// <summary>Texture / ImGui / 長寿命Buffer用のPersistent Descriptorを確保します。</summary>
		uint32_t Allocate();

		/// <summary>Persistent Descriptorを解放します。GPU参照中AssetはGpuDeferredReleaseQueue経由で解放してください。</summary>
		void Free(uint32_t srvIndex);

		/// <summary>
		/// 現在記録中Frame Resource専用領域から連続Descriptor Rangeを確保します。
		/// 同じFrame indexがFence安全化されて再利用されたときだけArena先頭へ戻ります。
		/// </summary>
		TransientDescriptorAllocation AllocateTransient(uint32_t count = 1);

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

		struct FrameTransientState
		{
			uint32_t firstIndex = 0;
			uint32_t capacity = 0;
			uint32_t cursor = 0;
			uint32_t highWater = 0;
			UINT64 observedFrameFenceValue = 0;
			bool initialized = false;
		};

		void RefreshTransientFrameLocked(uint32_t frameIndex);

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
		std::vector<FrameTransientState> transientFrameStates_;
		DescriptorStats descriptorStats_{};

	private: /// ---------- コピー禁止 ---------- ///

		SRVManager() = default;
		~SRVManager() = default;
		SRVManager(const SRVManager&) = delete;
		SRVManager& operator=(const SRVManager&) = delete;
	};


} // namespace Ken4lowEngine
