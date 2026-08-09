#include "DX12FenceManager.h"
#include <cassert>

namespace Ken4lowEngine
{

void DX12FenceManager::Initialize(ID3D12Device* device)
{
	HRESULT hr = device->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	fence_->SetName(L"DX12FenceManager Fence");
	assert(SUCCEEDED(hr));

	fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent_ != nullptr);
}

void DX12FenceManager::Finalize()
{
	if (fenceEvent_)
	{
		CloseHandle(fenceEvent_);
		fenceEvent_ = nullptr;
	}
	fence_.Reset();
	fenceValue_ = 0;
}

void DX12FenceManager::Signal(ID3D12CommandQueue* commandQueue)
{
	(void)SignalAndGetValue(commandQueue);
}

UINT64 DX12FenceManager::SignalAndGetValue(ID3D12CommandQueue* commandQueue)
{
	++fenceValue_;
	const HRESULT hr = commandQueue->Signal(fence_.Get(), fenceValue_);
	assert(SUCCEEDED(hr));
	return fenceValue_;
}

void DX12FenceManager::Wait()
{
	WaitForValue(fenceValue_);
}

void DX12FenceManager::WaitForValue(UINT64 fenceValue)
{
	if (!fence_ || fenceValue == 0 || fence_->GetCompletedValue() >= fenceValue)
	{
		return;
	}

	const HRESULT hr = fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
	assert(SUCCEEDED(hr));
	if (SUCCEEDED(hr))
	{
		WaitForSingleObject(fenceEvent_, INFINITE);
	}
}

UINT64 DX12FenceManager::GetCompletedValue() const
{
	return fence_ ? fence_->GetCompletedValue() : 0;
}

} // namespace Ken4lowEngine
