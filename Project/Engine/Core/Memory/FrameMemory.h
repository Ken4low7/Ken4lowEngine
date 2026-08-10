#pragma once

#include <cstddef>
#include <cstdint>
#include <memory_resource>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// 1フレーム内だけ生存する一時コンテナ向けLinear Memory Resource。
	/// BeginFrameで一括Resetするため、個別deallocateは行わない。
	/// </summary>
	class FrameMemory final : public std::pmr::memory_resource
	{
	public:
		struct Stats
		{
			std::size_t capacityBytes = 0;
			std::size_t currentUsedBytes = 0;
			std::size_t lastFrameUsedBytes = 0;
			std::size_t highWaterBytes = 0;
			std::size_t currentOverflowBytes = 0;
			std::size_t lastFrameOverflowBytes = 0;
		};

		static FrameMemory* GetInstance();

		void Initialize(std::size_t capacityBytes = 2 * 1024 * 1024);
		void Finalize();
		void BeginFrame();

		[[nodiscard]] std::pmr::memory_resource* GetMemoryResource() { return this; }
		[[nodiscard]] Stats GetStats() const;
		[[nodiscard]] bool IsInitialized() const { return initialized_; }

	private:
		struct OverflowBlock
		{
			void* pointer = nullptr;
			std::size_t bytes = 0;
			std::size_t alignment = alignof(std::max_align_t);
		};

		FrameMemory() = default;
		~FrameMemory() override;
		FrameMemory(const FrameMemory&) = delete;
		FrameMemory& operator=(const FrameMemory&) = delete;

		void ReleaseOverflowBlocks();
		void* do_allocate(std::size_t bytes, std::size_t alignment) override;
		void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override;
		bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override;

		std::vector<std::byte> buffer_;
		std::vector<OverflowBlock> overflowBlocks_;
		std::pmr::memory_resource* upstream_ = std::pmr::new_delete_resource();
		std::size_t offset_ = 0;
		std::size_t lastFrameUsedBytes_ = 0;
		std::size_t highWaterBytes_ = 0;
		std::size_t overflowBytes_ = 0;
		std::size_t lastFrameOverflowBytes_ = 0;
		bool initialized_ = false;
	};
} // namespace Ken4lowEngine
