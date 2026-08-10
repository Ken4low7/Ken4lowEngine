#include "FrameMemory.h"

#include <algorithm>
#include <cstdint>

namespace Ken4lowEngine
{
	FrameMemory* FrameMemory::GetInstance()
	{
		static FrameMemory instance;
		return &instance;
	}

	FrameMemory::~FrameMemory()
	{
		Finalize();
	}

	void FrameMemory::Initialize(std::size_t capacityBytes)
	{
		Finalize();
		capacityBytes = (std::max)(capacityBytes, std::size_t{ 64 * 1024 });
		buffer_.resize(capacityBytes);
		offset_ = 0;
		lastFrameUsedBytes_ = 0;
		highWaterBytes_ = 0;
		overflowBytes_ = 0;
		lastFrameOverflowBytes_ = 0;
		initialized_ = true;
	}

	void FrameMemory::Finalize()
	{
		ReleaseOverflowBlocks();
		buffer_.clear();
		buffer_.shrink_to_fit();
		offset_ = 0;
		lastFrameUsedBytes_ = 0;
		highWaterBytes_ = 0;
		overflowBytes_ = 0;
		lastFrameOverflowBytes_ = 0;
		initialized_ = false;
	}

	void FrameMemory::BeginFrame()
	{
		if (!initialized_) Initialize();
		lastFrameUsedBytes_ = offset_;
		lastFrameOverflowBytes_ = overflowBytes_;
		highWaterBytes_ = (std::max)(highWaterBytes_, offset_ + overflowBytes_);
		ReleaseOverflowBlocks();
		offset_ = 0;
		overflowBytes_ = 0;
	}

	FrameMemory::Stats FrameMemory::GetStats() const
	{
		return {
			buffer_.size(),
			offset_,
			lastFrameUsedBytes_,
			highWaterBytes_,
			overflowBytes_,
			lastFrameOverflowBytes_,
		};
	}

	void FrameMemory::ReleaseOverflowBlocks()
	{
		for (const OverflowBlock& block : overflowBlocks_)
		{
			if (block.pointer) upstream_->deallocate(block.pointer, block.bytes, block.alignment);
		}
		overflowBlocks_.clear();
	}

	void* FrameMemory::do_allocate(std::size_t bytes, std::size_t alignment)
	{
		if (!initialized_) Initialize();
		if (bytes == 0) bytes = 1;
		if (alignment == 0) alignment = alignof(std::max_align_t);

		const std::size_t mask = alignment - 1;
		const std::size_t alignedOffset = (offset_ + mask) & ~mask;
		if (alignedOffset <= buffer_.size() && bytes <= buffer_.size() - alignedOffset)
		{
			void* pointer = buffer_.data() + alignedOffset;
			offset_ = alignedOffset + bytes;
			highWaterBytes_ = (std::max)(highWaterBytes_, offset_ + overflowBytes_);
			return pointer;
		}

		void* pointer = upstream_->allocate(bytes, alignment);
		overflowBlocks_.push_back({ pointer, bytes, alignment });
		overflowBytes_ += bytes;
		highWaterBytes_ = (std::max)(highWaterBytes_, offset_ + overflowBytes_);
		return pointer;
	}

	void FrameMemory::do_deallocate(void*, std::size_t, std::size_t)
	{
		// Frame allocatorはBeginFrameで一括解放する。個別deallocateは意図的にno-op。
	}

	bool FrameMemory::do_is_equal(const std::pmr::memory_resource& other) const noexcept
	{
		return this == &other;
	}
} // namespace Ken4lowEngine
