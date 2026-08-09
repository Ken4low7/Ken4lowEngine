#include "DX12CommandManager.h"
#include "DX12FenceManager.h"

#include <cassert>
#include <chrono>
#include <cwchar>

namespace Ken4lowEngine
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		float ToMilliseconds(const Clock::time_point& begin)
		{
			return std::chrono::duration<float, std::milli>(Clock::now() - begin).count();
		}
	}

	void DX12CommandManager::Initialize(ID3D12Device* device)
	{
		HRESULT hr{};
		commandListSubmitted_ = false;
		performanceTiming_ = {};
		currentFrameIndex_ = 0;
		frameResources_.clear();

		hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
		commandAllocator_->SetName(L"Main Command Allocator Bootstrap");
		assert(SUCCEEDED(hr));

		hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
		commandList_->SetName(L"Main Command List");
		assert(SUCCEEDED(hr));

		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
		commandQueue_->SetName(L"Main Command Queue");
		assert(SUCCEEDED(hr));
	}

	void DX12CommandManager::ConfigureFramesInFlight(ID3D12Device* device, uint32_t frameCount, uint32_t initialFrameIndex)
	{
		if (!device || frameCount == 0)
		{
			return;
		}

		initialFrameIndex = initialFrameIndex < frameCount ? initialFrameIndex : 0;
		frameResources_.clear();
		frameResources_.resize(frameCount);

		for (uint32_t index = 0; index < frameCount; ++index)
		{
			FrameResource& frame = frameResources_[index];
			if (index == initialFrameIndex)
			{
				frame.commandAllocator = commandAllocator_;
			}
			else
			{
				const HRESULT hr = device->CreateCommandAllocator(
					D3D12_COMMAND_LIST_TYPE_DIRECT,
					IID_PPV_ARGS(&frame.commandAllocator));
				assert(SUCCEEDED(hr));
			}

			wchar_t name[64]{};
			swprintf_s(name, L"Main Command Allocator Frame %u", index);
			frame.commandAllocator->SetName(name);
		}

		currentFrameIndex_ = initialFrameIndex;
		commandAllocator_ = frameResources_[currentFrameIndex_].commandAllocator;
	}

	void DX12CommandManager::Finalize()
	{
		assert(!commandListSubmitted_ && "DirectXCommon must finish submitted GPU work before finalizing the command manager.");
		commandQueue_.Reset();
		commandList_.Reset();
		commandAllocator_.Reset();
		frameResources_.clear();
	}

	void DX12CommandManager::ResourceTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
	{
		if (stateBefore == stateAfter) return;

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = stateBefore;
		barrier.Transition.StateAfter = stateAfter;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList_->ResourceBarrier(1, &barrier);
	}

	void DX12CommandManager::ExecuteAndWait()
	{
		Execute();
		WaitAndReset();
	}

	void DX12CommandManager::Execute()
	{
		assert(!commandListSubmitted_ && "The submitted command list must be prepared before submitting again.");
		if (commandListSubmitted_) return;

		performanceTiming_ = {};

		const auto closeBegin = Clock::now();
		const HRESULT hr = commandList_->Close();
		performanceTiming_.commandListCloseMs = ToMilliseconds(closeBegin);
		assert(SUCCEEDED(hr));
		if (FAILED(hr)) return;

		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		const auto executeBegin = Clock::now();
		commandQueue_->ExecuteCommandLists(1, commandLists);
		performanceTiming_.executeCommandListsMs = ToMilliseconds(executeBegin);
		commandListSubmitted_ = true;
	}

	void DX12CommandManager::SignalCurrentFrame(uint32_t frameIndex)
	{
		if (!commandListSubmitted_ || !fenceManager_)
		{
			return;
		}

		const auto signalBegin = Clock::now();
		const UINT64 fenceValue = fenceManager_->SignalAndGetValue(commandQueue_.Get());
		performanceTiming_.fenceSignalMs = ToMilliseconds(signalBegin);

		if (IsValidFrameIndex(frameIndex))
		{
			frameResources_[frameIndex].fenceValue = fenceValue;
		}
	}

	void DX12CommandManager::PrepareFrame(uint32_t frameIndex)
	{
		if (!commandListSubmitted_ || !fenceManager_ || !IsValidFrameIndex(frameIndex))
		{
			return;
		}

		FrameResource& nextFrame = frameResources_[frameIndex];

		const auto waitBegin = Clock::now();
		fenceManager_->WaitForValue(nextFrame.fenceValue);
		performanceTiming_.fenceWaitMs = ToMilliseconds(waitBegin);

		commandAllocator_ = nextFrame.commandAllocator;
		const auto allocatorResetBegin = Clock::now();
		const HRESULT allocatorResult = commandAllocator_->Reset();
		performanceTiming_.allocatorResetMs = ToMilliseconds(allocatorResetBegin);
		assert(SUCCEEDED(allocatorResult));
		if (FAILED(allocatorResult)) return;

		const auto commandListResetBegin = Clock::now();
		const HRESULT commandListResult = commandList_->Reset(commandAllocator_.Get(), nullptr);
		performanceTiming_.commandListResetMs = ToMilliseconds(commandListResetBegin);
		assert(SUCCEEDED(commandListResult));
		if (FAILED(commandListResult)) return;

		currentFrameIndex_ = frameIndex;
		commandListSubmitted_ = false; // 次に再利用するFrameResourceの準備が終わった時点で記録可能へ戻す。
	}

	void DX12CommandManager::WaitAndReset()
	{
		if (!commandListSubmitted_ || !fenceManager_) return;

		const auto signalBegin = Clock::now();
		const UINT64 fenceValue = fenceManager_->SignalAndGetValue(commandQueue_.Get());
		performanceTiming_.fenceSignalMs = ToMilliseconds(signalBegin);

		if (IsValidFrameIndex(currentFrameIndex_))
		{
			frameResources_[currentFrameIndex_].fenceValue = fenceValue;
		}

		const auto waitBegin = Clock::now();
		fenceManager_->WaitForValue(fenceValue);
		performanceTiming_.fenceWaitMs = ToMilliseconds(waitBegin);

		const auto allocatorResetBegin = Clock::now();
		const HRESULT allocatorResult = commandAllocator_->Reset();
		performanceTiming_.allocatorResetMs = ToMilliseconds(allocatorResetBegin);
		assert(SUCCEEDED(allocatorResult));
		if (FAILED(allocatorResult)) return;

		const auto commandListResetBegin = Clock::now();
		const HRESULT commandListResult = commandList_->Reset(commandAllocator_.Get(), nullptr);
		performanceTiming_.commandListResetMs = ToMilliseconds(commandListResetBegin);
		assert(SUCCEEDED(commandListResult));
		if (SUCCEEDED(commandListResult)) commandListSubmitted_ = false;
	}

} // namespace Ken4lowEngine
