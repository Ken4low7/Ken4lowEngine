#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>

#ifdef _DEBUG
#include <crtdbg.h>
#endif // _DEBUG

namespace Ken4lowEngine
{
	struct FrameAllocationStats
	{
		uint64_t allocationCount = 0;
		uint64_t allocatedBytes = 0;
		uint64_t peakAllocationCount = 0;
		uint64_t peakAllocatedBytes = 0;
	};

	/// <summary>
	/// Debug CRTのAllocation Hookを利用して、直前に完了した1フレームの
	/// Allocation要求回数と要求バイト数を観測する軽量トラッカー。
	/// Releaseでは計測を無効化し、呼び出し側のコードを共通化する。
	/// </summary>
	class FrameAllocationTracker
	{
	public:
		static FrameAllocationTracker* GetInstance()
		{
			static FrameAllocationTracker instance;
			return &instance;
		}

		FrameAllocationTracker(const FrameAllocationTracker&) = delete;
		FrameAllocationTracker& operator=(const FrameAllocationTracker&) = delete;

		void Initialize()
		{
			if (initialized_)
			{
				return;
			}

			initialized_ = true;
#ifdef _DEBUG
			previousHook_ = _CrtSetAllocHook(&FrameAllocationTracker::AllocationHook);
			supported_ = true;
#else
			supported_ = false;
#endif // _DEBUG
		}

		void Finalize()
		{
			if (!initialized_)
			{
				return;
			}

			frameActive_.store(false, std::memory_order_release);
#ifdef _DEBUG
			// Engine導入前に存在したHookへ戻し、外部のDebug CRT計測を壊さない。
			_CrtSetAllocHook(previousHook_);
			previousHook_ = nullptr;
#endif // _DEBUG
			supported_ = false;
			initialized_ = false;
		}

		void BeginFrame()
		{
			if (!supported_)
			{
				return;
			}

			currentAllocationCount_.store(0, std::memory_order_relaxed);
			currentAllocatedBytes_.store(0, std::memory_order_relaxed);
			frameActive_.store(true, std::memory_order_release);
		}

		void EndFrame()
		{
			if (!supported_)
			{
				return;
			}

			frameActive_.store(false, std::memory_order_release);

			// Hook内で既に計測を開始したWorker Threadが完了してから値を確定する。
			while (activeHookCalls_.load(std::memory_order_acquire) != 0)
			{
				std::this_thread::yield();
			}

			const uint64_t allocationCount = currentAllocationCount_.exchange(0, std::memory_order_relaxed);
			const uint64_t allocatedBytes = currentAllocatedBytes_.exchange(0, std::memory_order_relaxed);
			lastAllocationCount_.store(allocationCount, std::memory_order_relaxed);
			lastAllocatedBytes_.store(allocatedBytes, std::memory_order_relaxed);

			UpdatePeak(peakAllocationCount_, allocationCount);
			UpdatePeak(peakAllocatedBytes_, allocatedBytes);
		}

		void ResetPeaks()
		{
			peakAllocationCount_.store(0, std::memory_order_relaxed);
			peakAllocatedBytes_.store(0, std::memory_order_relaxed);
		}

		FrameAllocationStats GetLastFrameStats() const
		{
			FrameAllocationStats stats{};
			stats.allocationCount = lastAllocationCount_.load(std::memory_order_relaxed);
			stats.allocatedBytes = lastAllocatedBytes_.load(std::memory_order_relaxed);
			stats.peakAllocationCount = peakAllocationCount_.load(std::memory_order_relaxed);
			stats.peakAllocatedBytes = peakAllocatedBytes_.load(std::memory_order_relaxed);
			return stats;
		}

		bool IsSupported() const { return supported_; }

	private:
		FrameAllocationTracker() = default;
		~FrameAllocationTracker() = default;

		static void UpdatePeak(std::atomic<uint64_t>& peak, uint64_t value)
		{
			uint64_t currentPeak = peak.load(std::memory_order_relaxed);
			while (value > currentPeak &&
				!peak.compare_exchange_weak(currentPeak, value, std::memory_order_relaxed, std::memory_order_relaxed))
			{
			}
		}

#ifdef _DEBUG
		static int __cdecl AllocationHook(
			int allocType,
			void* userData,
			size_t size,
			int blockType,
			long requestNumber,
			const unsigned char* fileName,
			int lineNumber)
		{
			FrameAllocationTracker* tracker = GetInstance();

			if (tracker->previousHook_ != nullptr)
			{
				const int previousResult = tracker->previousHook_(
					allocType,
					userData,
					size,
					blockType,
					requestNumber,
					fileName,
					lineNumber);
				if (previousResult == 0)
				{
					return 0;
				}
			}

			if ((allocType != _HOOK_ALLOC && allocType != _HOOK_REALLOC) || blockType == _CRT_BLOCK)
			{
				return 1;
			}

			if (!tracker->frameActive_.load(std::memory_order_acquire))
			{
				return 1;
			}

			tracker->activeHookCalls_.fetch_add(1, std::memory_order_acq_rel);
			if (tracker->frameActive_.load(std::memory_order_acquire))
			{
				tracker->currentAllocationCount_.fetch_add(1, std::memory_order_relaxed);
				tracker->currentAllocatedBytes_.fetch_add(static_cast<uint64_t>(size), std::memory_order_relaxed);
			}
			tracker->activeHookCalls_.fetch_sub(1, std::memory_order_acq_rel);
			return 1;
		}
#endif // _DEBUG

	private:
		bool initialized_ = false;
		bool supported_ = false;
		std::atomic<bool> frameActive_{ false };
		std::atomic<uint32_t> activeHookCalls_{ 0 };
		std::atomic<uint64_t> currentAllocationCount_{ 0 };
		std::atomic<uint64_t> currentAllocatedBytes_{ 0 };
		std::atomic<uint64_t> lastAllocationCount_{ 0 };
		std::atomic<uint64_t> lastAllocatedBytes_{ 0 };
		std::atomic<uint64_t> peakAllocationCount_{ 0 };
		std::atomic<uint64_t> peakAllocatedBytes_{ 0 };

#ifdef _DEBUG
		_CRT_ALLOC_HOOK previousHook_ = nullptr;
#endif // _DEBUG
	};
} // namespace Ken4lowEngine
