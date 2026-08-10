#pragma once

#include "DX12Include.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class SRVManager;

	/// <summary>
	/// GPUが参照中のResource / SRV / Model所有権をFence完了まで保持する遅延解放Queue。
	/// Unload直後にDescriptorを再利用して描画中GPUから参照される事故を防ぐ。
	/// </summary>
	class GpuDeferredReleaseQueue
	{
	public:
		static GpuDeferredReleaseQueue* GetInstance();

		void Initialize(DirectXCommon* dxCommon, SRVManager* srvManager);
		void Finalize();
		bool IsInitialized() const { return dxCommon_ != nullptr && srvManager_ != nullptr; }

		void EnqueueResource(ComPtr<ID3D12Resource> resource, uint32_t srvIndex = UINT32_MAX);
		void EnqueueKeepAlive(std::shared_ptr<void> keepAlive);
		void Collect();

		/// 終了時はCommandListをGPUへ送り切ってから残りを全解放する。
		void WaitAndFlush();
		size_t GetPendingCount() const;

	private:
		struct PendingRelease
		{
			ComPtr<ID3D12Resource> resource;
			std::shared_ptr<void> keepAlive;
			uint32_t srvIndex = UINT32_MAX;
			UINT64 retireFenceValue = 0;
		};

		UINT64 MakeRetireFenceValue() const;
		void ReleaseEntry(PendingRelease& entry);

	private:
		DirectXCommon* dxCommon_ = nullptr;
		SRVManager* srvManager_ = nullptr;
		mutable std::mutex mutex_;
		std::vector<PendingRelease> pending_;

	private:
		GpuDeferredReleaseQueue() = default;
		~GpuDeferredReleaseQueue() = default;
		GpuDeferredReleaseQueue(const GpuDeferredReleaseQueue&) = delete;
		GpuDeferredReleaseQueue& operator=(const GpuDeferredReleaseQueue&) = delete;
	};
} // namespace Ken4lowEngine
