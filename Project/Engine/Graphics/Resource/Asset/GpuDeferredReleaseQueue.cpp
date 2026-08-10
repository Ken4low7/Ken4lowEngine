#include "GpuDeferredReleaseQueue.h"

#include "DirectXCommon.h"
#include "SRVManager.h"

#include <algorithm>

namespace Ken4lowEngine
{
	GpuDeferredReleaseQueue* GpuDeferredReleaseQueue::GetInstance()
	{
		static GpuDeferredReleaseQueue instance;
		return &instance;
	}

	void GpuDeferredReleaseQueue::Initialize(DirectXCommon* dxCommon, SRVManager* srvManager)
	{
		WaitAndFlush();
		dxCommon_ = dxCommon;
		srvManager_ = srvManager;
	}

	void GpuDeferredReleaseQueue::Finalize()
	{
		WaitAndFlush();
		dxCommon_ = nullptr;
		srvManager_ = nullptr;
	}

	void GpuDeferredReleaseQueue::EnqueueResource(ComPtr<ID3D12Resource> resource, uint32_t srvIndex)
	{
		if (!resource && srvIndex == UINT32_MAX) return;
		if (!IsInitialized())
		{
			if (srvManager_ && srvIndex != UINT32_MAX) srvManager_->Free(srvIndex);
			return;
		}

		PendingRelease entry{};
		entry.resource = std::move(resource);
		entry.srvIndex = srvIndex;
		entry.retireFenceValue = MakeRetireFenceValue();
		std::scoped_lock lock(mutex_);
		pending_.push_back(std::move(entry));
	}

	void GpuDeferredReleaseQueue::EnqueueKeepAlive(std::shared_ptr<void> keepAlive)
	{
		if (!keepAlive) return;
		if (!IsInitialized()) return;

		PendingRelease entry{};
		entry.keepAlive = std::move(keepAlive);
		entry.retireFenceValue = MakeRetireFenceValue();
		std::scoped_lock lock(mutex_);
		pending_.push_back(std::move(entry));
	}

	void GpuDeferredReleaseQueue::Collect()
	{
		if (!IsInitialized() || !dxCommon_->GetFenceManager()) return;
		const UINT64 completedValue = dxCommon_->GetFenceManager()->GetCompletedValue();

		std::scoped_lock lock(mutex_);
		std::erase_if(pending_, [this, completedValue](PendingRelease& entry)
			{
				if (completedValue < entry.retireFenceValue) return false;
				ReleaseEntry(entry);
				return true;
			});
	}

	void GpuDeferredReleaseQueue::WaitAndFlush()
	{
		if (dxCommon_ && dxCommon_->GetCommandManager() && dxCommon_->GetFenceManager())
		{
			// 最終フレームの未送信Commandも含めてGPU完了を保証してから所有権を手放す。
			dxCommon_->GetCommandManager()->ExecuteAndWait();
		}

		std::scoped_lock lock(mutex_);
		for (PendingRelease& entry : pending_) ReleaseEntry(entry);
		pending_.clear();
	}

	size_t GpuDeferredReleaseQueue::GetPendingCount() const
	{
		std::scoped_lock lock(mutex_);
		return pending_.size();
	}

	UINT64 GpuDeferredReleaseQueue::MakeRetireFenceValue() const
	{
		if (!dxCommon_ || !dxCommon_->GetFenceManager()) return 0;
		// 現在記録中のCommandListがこのAssetを参照する可能性があるため、次回Signal以降まで保持する。
		return dxCommon_->GetFenceManager()->GetCurrentValue() + 1;
	}

	void GpuDeferredReleaseQueue::ReleaseEntry(PendingRelease& entry)
	{
		if (srvManager_ && entry.srvIndex != UINT32_MAX)
		{
			srvManager_->Free(entry.srvIndex);
			entry.srvIndex = UINT32_MAX;
		}
		entry.resource.Reset();
		entry.keepAlive.reset();
	}
} // namespace Ken4lowEngine
