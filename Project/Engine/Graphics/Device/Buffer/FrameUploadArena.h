#pragma once

#include "DX12Include.h"
#include <ResourceManager.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// FrameResourceごとに独立したUpload Heapを持つLinear Allocator。
	/// Fence完了済みFrameだけをBeginFrameでResetすることでCPU/GPUの並行実行中も上書きを防ぐ。
	/// </summary>
	class FrameUploadArena
	{
	public:
		struct Allocation
		{
			void* cpuAddress = nullptr;
			D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;
			std::size_t sizeBytes = 0;

			[[nodiscard]] bool IsValid() const
			{
				return cpuAddress != nullptr && gpuAddress != 0;
			}
		};

		struct Stats
		{
			std::size_t capacityBytes = 0;
			std::size_t usedBytes = 0;
			std::size_t highWaterBytes = 0;
			std::size_t overflowBytes = 0;
			std::size_t overflowAllocationCount = 0;
			uint32_t frameCount = 0;
			uint32_t currentFrameIndex = 0;
		};

		void Initialize(
			ID3D12Device* device,
			uint32_t frameCount,
			std::size_t capacityBytesPerFrame = 4u * 1024u * 1024u)
		{
			Finalize();
			if (!device)
			{
				return;
			}

			device_ = device;
			capacityBytesPerFrame_ = (std::max)(capacityBytesPerFrame, std::size_t{ 64u * 1024u });
			frames_.resize((std::max)(1u, frameCount));

			for (std::size_t frameIndex = 0; frameIndex < frames_.size(); ++frameIndex)
			{
				FrameData& frame = frames_[frameIndex];
				frame.resource = ResourceManager::CreateBufferResource(device_, capacityBytesPerFrame_);
				assert(frame.resource != nullptr);
				if (!frame.resource)
				{
					continue;
				}

				const HRESULT hr = frame.resource->Map(0, nullptr, reinterpret_cast<void**>(&frame.mappedData));
				assert(SUCCEEDED(hr));

				wchar_t resourceName[64]{};
				swprintf_s(resourceName, L"Frame Upload Arena %u", static_cast<unsigned int>(frameIndex));
				frame.resource->SetName(resourceName);
			}

			currentFrameIndex_ = 0;
		}

		void Finalize()
		{
			for (FrameData& frame : frames_)
			{
				ReleaseOverflow(frame);
				if (frame.resource && frame.mappedData)
				{
					frame.resource->Unmap(0, nullptr);
				}
				frame.mappedData = nullptr;
				frame.resource.Reset();
				frame.offsetBytes = 0;
				frame.highWaterBytes = 0;
			}

			frames_.clear();
			device_ = nullptr;
			capacityBytesPerFrame_ = 0;
			currentFrameIndex_ = 0;
		}

		void BeginFrame(uint32_t frameIndex)
		{
			if (frames_.empty())
			{
				return;
			}

			currentFrameIndex_ = frameIndex % static_cast<uint32_t>(frames_.size());
			FrameData& frame = frames_[currentFrameIndex_];
			ReleaseOverflow(frame);
			frame.offsetBytes = 0; // このFrameのFence完了後だけResetし、GPU参照中の領域を再利用しない。
			frame.overflowBytes = 0;
			frame.overflowAllocationCount = 0;
		}

		Allocation Allocate(std::size_t sizeBytes, std::size_t alignment = alignof(std::max_align_t))
		{
			if (frames_.empty() || !device_ || sizeBytes == 0)
			{
				return {};
			}

			alignment = (std::max)(std::size_t{ 1 }, alignment);
			FrameData& frame = frames_[currentFrameIndex_];
			const std::size_t alignedOffset = AlignUp(frame.offsetBytes, alignment);
			if (frame.resource && frame.mappedData &&
				alignedOffset <= capacityBytesPerFrame_ &&
				sizeBytes <= capacityBytesPerFrame_ - alignedOffset)
			{
				Allocation allocation{};
				allocation.cpuAddress = frame.mappedData + alignedOffset;
				allocation.gpuAddress = frame.resource->GetGPUVirtualAddress() + alignedOffset;
				allocation.sizeBytes = sizeBytes;
				frame.offsetBytes = alignedOffset + sizeBytes;
				frame.highWaterBytes = (std::max)(frame.highWaterBytes, frame.offsetBytes);
				return allocation;
			}

			return AllocateOverflow(frame, sizeBytes, alignment);
		}

		template <class T>
		Allocation AllocateConstant(const T& value)
		{
			Allocation allocation = Allocate(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			if (allocation.IsValid())
			{
				std::memcpy(allocation.cpuAddress, &value, sizeof(T));
			}
			return allocation;
		}

		[[nodiscard]] Stats GetStats() const
		{
			Stats stats{};
			stats.capacityBytes = capacityBytesPerFrame_;
			stats.frameCount = static_cast<uint32_t>(frames_.size());
			stats.currentFrameIndex = currentFrameIndex_;
			if (!frames_.empty())
			{
				const FrameData& frame = frames_[currentFrameIndex_];
				stats.usedBytes = frame.offsetBytes;
				stats.highWaterBytes = frame.highWaterBytes;
				stats.overflowBytes = frame.overflowBytes;
				stats.overflowAllocationCount = frame.overflowAllocationCount;
			}
			return stats;
		}

	private:
		struct OverflowBlock
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			std::byte* mappedData = nullptr;
			std::size_t sizeBytes = 0;
		};

		struct FrameData
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			std::byte* mappedData = nullptr;
			std::size_t offsetBytes = 0;
			std::size_t highWaterBytes = 0;
			std::size_t overflowBytes = 0;
			std::size_t overflowAllocationCount = 0;
			std::vector<OverflowBlock> overflowBlocks;
		};

		static std::size_t AlignUp(std::size_t value, std::size_t alignment)
		{
			const std::size_t remainder = value % alignment;
			return remainder == 0 ? value : value + (alignment - remainder);
		}

		Allocation AllocateOverflow(FrameData& frame, std::size_t sizeBytes, std::size_t alignment)
		{
			const std::size_t resourceBytes = AlignUp(sizeBytes, alignment);
			OverflowBlock block{};
			block.resource = ResourceManager::CreateBufferResource(device_, resourceBytes);
			if (!block.resource)
			{
				return {};
			}

			const HRESULT hr = block.resource->Map(0, nullptr, reinterpret_cast<void**>(&block.mappedData));
			assert(SUCCEEDED(hr));
			if (FAILED(hr) || !block.mappedData)
			{
				return {};
			}

			block.sizeBytes = resourceBytes;
			block.resource->SetName(L"Frame Upload Arena Overflow");

			Allocation allocation{};
			allocation.cpuAddress = block.mappedData;
			allocation.gpuAddress = block.resource->GetGPUVirtualAddress();
			allocation.sizeBytes = sizeBytes;
			frame.overflowBytes += resourceBytes;
			++frame.overflowAllocationCount;
			frame.overflowBlocks.push_back(std::move(block));
			return allocation;
		}

		static void ReleaseOverflow(FrameData& frame)
		{
			for (OverflowBlock& block : frame.overflowBlocks)
			{
				if (block.resource && block.mappedData)
				{
					block.resource->Unmap(0, nullptr);
				}
				block.mappedData = nullptr;
				block.resource.Reset();
			}
			frame.overflowBlocks.clear();
		}

	private:
		ID3D12Device* device_ = nullptr;
		std::vector<FrameData> frames_;
		std::size_t capacityBytesPerFrame_ = 0;
		uint32_t currentFrameIndex_ = 0;
	};
} // namespace Ken4lowEngine
