#pragma once

#include <Engine/Core/Concurrency/JobSystem.h>

#include <any>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{
	enum class StreamingPriority : uint8_t
	{
		Background = 0,
		Normal,
		High,
		Critical,
	};

	struct StreamingRequestState
	{
		std::atomic_bool canceled{ false };
	};

	class StreamingRequestHandle
	{
	public:
		[[nodiscard]] bool IsValid() const { return id_ != 0 && state_ != nullptr; }
		[[nodiscard]] uint64_t GetId() const { return id_; }
		[[nodiscard]] bool IsCanceled() const { return !state_ || state_->canceled.load(std::memory_order_acquire); }
		void Cancel() const { if (state_) state_->canceled.store(true, std::memory_order_release); }

	private:
		StreamingRequestHandle(uint64_t id, std::shared_ptr<StreamingRequestState> state)
			: id_(id), state_(std::move(state)) {}

		uint64_t id_ = 0;
		std::shared_ptr<StreamingRequestState> state_;
		friend class StreamingManager;
	};

	/// <summary>
	/// Worker側のIO/DecodeとMain Thread側のCommitを分離する共通Streaming Queue。
	/// BackgroundTaskはD3D12やActorWorldを直接変更せず、Completion側でEngine状態へ反映する。
	/// </summary>
	class StreamingManager
	{
	public:
		using Payload = std::any;
		using BackgroundTask = std::function<Payload()>;
		using CompletionTask = std::function<void(Payload&&, std::exception_ptr)>;

		static StreamingManager* GetInstance();

		void Initialize();
		void Finalize();

		StreamingRequestHandle Request(
			BackgroundTask backgroundTask,
			CompletionTask completionTask,
			StreamingPriority priority = StreamingPriority::Normal);

		bool Cancel(uint64_t requestId);
		void Update(std::size_t maxCompletionsPerFrame = 4);

		[[nodiscard]] bool IsInitialized() const { return initialized_.load(std::memory_order_acquire); }
		[[nodiscard]] std::size_t GetPendingRequestCount() const;
		[[nodiscard]] std::size_t GetQueuedCompletionCount() const;

	private:
		struct Completion
		{
			uint64_t requestId = 0;
			std::shared_ptr<StreamingRequestState> state;
			CompletionTask callback;
			Payload payload;
			std::exception_ptr exception;
		};

		StreamingManager() = default;
		~StreamingManager();
		StreamingManager(const StreamingManager&) = delete;
		StreamingManager& operator=(const StreamingManager&) = delete;

		static JobPriority ToJobPriority(StreamingPriority priority);

		mutable std::mutex mutex_;
		std::unordered_map<uint64_t, std::shared_ptr<StreamingRequestState>> requests_;
		std::vector<Completion> completions_;
		std::atomic_uint64_t nextRequestId_{ 1 };
		std::atomic_bool initialized_{ false };
		std::atomic_bool acceptingRequests_{ false };
	};
} // namespace Ken4lowEngine
