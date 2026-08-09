#pragma once
#include "DX12Include.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class DX12FenceManager;

class DX12CommandManager
{
public:
	struct PerformanceTiming
	{
		float commandListCloseMs = 0.0f;
		float executeCommandListsMs = 0.0f;
		float fenceSignalMs = 0.0f;
		float fenceWaitMs = 0.0f;
		float allocatorResetMs = 0.0f;
		float commandListResetMs = 0.0f;
	};

	void Initialize(ID3D12Device* device);
	void Finalize();

	/// SwapChain生成後にBackBuffer数と同数のAllocatorを用意し、CPU/GPUの複数Frame並行を有効化する。
	void ConfigureFramesInFlight(ID3D12Device* device, uint32_t frameCount, uint32_t initialFrameIndex);

	void ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

	/// UploadやPicking等、呼び出し元が即時完了を必要とする処理用の完全同期経路。
	void ExecuteAndWait();

	/// 現在のCommandListをCloseしてQueueへ送信する。
	void Execute();

	/// 通常フレーム用: 送信済みFrameにFence値を記録する。
	void SignalCurrentFrame(uint32_t frameIndex);

	/// 通常フレーム用: 次に再利用するFrameResourceだけを必要に応じて待機し、記録を再開する。
	void PrepareFrame(uint32_t frameIndex);

	/// 従来互換の完全待機。ExecuteAndWait内部でも使用する。
	void WaitAndReset();

	void SetFenceManager(DX12FenceManager* fenceManager) { fenceManager_ = fenceManager; }

	ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return commandAllocator_.Get(); }
	ID3D12CommandQueue* GetCommandQueue() const { return commandQueue_.Get(); }
	const PerformanceTiming& GetPerformanceTiming() const { return performanceTiming_; }
	uint32_t GetFrameResourceCount() const { return static_cast<uint32_t>(frameResources_.size()); }
	uint32_t GetCurrentFrameIndex() const { return currentFrameIndex_; }

private:
	struct FrameResource
	{
		ComPtr<ID3D12CommandAllocator> commandAllocator;
		UINT64 fenceValue = 0;
	};

	bool IsValidFrameIndex(uint32_t frameIndex) const
	{
		return frameIndex < frameResources_.size();
	}

private:
	DX12FenceManager* fenceManager_ = nullptr;

	ComPtr<ID3D12CommandAllocator> commandAllocator_; // 現在記録中FrameのAllocator参照。
	ComPtr<ID3D12GraphicsCommandList> commandList_;
	ComPtr<ID3D12CommandQueue> commandQueue_;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	std::vector<FrameResource> frameResources_;
	uint32_t currentFrameIndex_ = 0;
	bool commandListSubmitted_ = false;
	PerformanceTiming performanceTiming_{};
};

} // namespace Ken4lowEngine
