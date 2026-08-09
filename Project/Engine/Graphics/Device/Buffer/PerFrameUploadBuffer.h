#pragma once

#include "DX12Include.h"
#include <ResourceManager.h>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	template <class T>
	class PerFrameUploadBuffer
	{
	public:
		void Initialize(ID3D12Device* device, uint32_t frameCount)
		{
			Finalize();
			if (!device)
			{
				return;
			}

			frameCount = (std::max)(1u, frameCount);
			frames_.resize(frameCount);
			for (FrameBuffer& frame : frames_)
			{
				frame.resource = ResourceManager::CreateBufferResource(device, sizeof(T));
				if (!frame.resource)
				{
					continue;
				}

				frame.resource->Map(0, nullptr, reinterpret_cast<void**>(&frame.mappedData));
			}
		}

		void Finalize()
		{
			for (FrameBuffer& frame : frames_)
			{
				if (frame.resource && frame.mappedData)
				{
					frame.resource->Unmap(0, nullptr);
				}
				frame.mappedData = nullptr;
				frame.resource.Reset();
			}
			frames_.clear();
		}

		void WriteFrame(uint32_t frameIndex, const T& value)
		{
			T* mappedData = GetMappedData(frameIndex);
			if (mappedData)
			{
				*mappedData = value; // GPUが別FrameのBufferを読む間は、現在Frame専用のUpload Bufferだけを書き換える。
			}
		}

		void WriteAll(const T& value)
		{
			for (FrameBuffer& frame : frames_)
			{
				if (frame.mappedData)
				{
					*frame.mappedData = value;
				}
			}
		}

		T* GetMappedData(uint32_t frameIndex)
		{
			FrameBuffer* frame = GetFrame(frameIndex);
			return frame ? frame->mappedData : nullptr;
		}

		const T* GetMappedData(uint32_t frameIndex) const
		{
			const FrameBuffer* frame = GetFrame(frameIndex);
			return frame ? frame->mappedData : nullptr;
		}

		Microsoft::WRL::ComPtr<ID3D12Resource> GetResource(uint32_t frameIndex) const
		{
			const FrameBuffer* frame = GetFrame(frameIndex);
			return frame ? frame->resource : nullptr;
		}

		D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress(uint32_t frameIndex) const
		{
			const FrameBuffer* frame = GetFrame(frameIndex);
			return frame && frame->resource ? frame->resource->GetGPUVirtualAddress() : 0;
		}

		uint32_t GetFrameCount() const
		{
			return static_cast<uint32_t>(frames_.size());
		}

	private:
		struct FrameBuffer
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			T* mappedData = nullptr;
		};

		FrameBuffer* GetFrame(uint32_t frameIndex)
		{
			if (frames_.empty())
			{
				return nullptr;
			}
			return &frames_[frameIndex % frames_.size()];
		}

		const FrameBuffer* GetFrame(uint32_t frameIndex) const
		{
			if (frames_.empty())
			{
				return nullptr;
			}
			return &frames_[frameIndex % frames_.size()];
		}

	private:
		std::vector<FrameBuffer> frames_;
	};
} // namespace Ken4lowEngine
