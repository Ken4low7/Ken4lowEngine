#include "StreamingManager.h"

#include <algorithm>
#include <utility>

namespace Ken4lowEngine
{
	StreamingManager* StreamingManager::GetInstance()
	{
		static StreamingManager instance;
		return &instance;
	}

	StreamingManager::~StreamingManager()
	{
		Finalize();
	}

	void StreamingManager::Initialize()
	{
		if (IsInitialized()) return;
		JobSystem::GetInstance()->Initialize();
		{
			std::scoped_lock lock(mutex_);
			requests_.clear();
			completions_.clear();
		}
		acceptingRequests_.store(true, std::memory_order_release);
		initialized_.store(true, std::memory_order_release);
	}

	void StreamingManager::Finalize()
	{
		if (!IsInitialized()) return;
		acceptingRequests_.store(false, std::memory_order_release);

		{
			std::scoped_lock lock(mutex_);
			for (auto& [id, state] : requests_)
			{
				(void)id;
				if (state) state->canceled.store(true, std::memory_order_release);
			}
		}

		// WorkerがthisへCompletionを積む可能性があるため、Queueを消す前に全Jobの終了を待つ。
		JobSystem::GetInstance()->WaitIdle();
		{
			std::scoped_lock lock(mutex_);
			requests_.clear();
			completions_.clear();
		}
		initialized_.store(false, std::memory_order_release);
	}

	StreamingRequestHandle StreamingManager::Request(
		BackgroundTask backgroundTask,
		CompletionTask completionTask,
		StreamingPriority priority)
	{
		if (!backgroundTask || !completionTask) return {};
		if (!IsInitialized()) Initialize();
		if (!acceptingRequests_.load(std::memory_order_acquire)) return {};

		const uint64_t requestId = nextRequestId_.fetch_add(1, std::memory_order_relaxed);
		auto state = std::make_shared<StreamingRequestState>();
		{
			std::scoped_lock lock(mutex_);
			requests_[requestId] = state;
		}

		JobSystem::GetInstance()->Dispatch(
			[this, requestId, state, backgroundTask = std::move(backgroundTask), completionTask = std::move(completionTask)]() mutable
			{
				Payload payload;
				std::exception_ptr exception;
				if (!state->canceled.load(std::memory_order_acquire))
				{
					try
					{
						payload = backgroundTask();
					}
					catch (...)
					{
						exception = std::current_exception();
					}
				}

				std::scoped_lock lock(mutex_);
				completions_.push_back({
					requestId,
					state,
					std::move(completionTask),
					std::move(payload),
					exception,
				});
			},
			ToJobPriority(priority));

		return StreamingRequestHandle(requestId, std::move(state));
	}

	bool StreamingManager::Cancel(uint64_t requestId)
	{
		std::scoped_lock lock(mutex_);
		const auto found = requests_.find(requestId);
		if (found == requests_.end() || !found->second) return false;
		found->second->canceled.store(true, std::memory_order_release);
		return true;
	}

	void StreamingManager::Update(std::size_t maxCompletionsPerFrame)
	{
		if (!IsInitialized() || maxCompletionsPerFrame == 0) return;

		for (std::size_t processed = 0; processed < maxCompletionsPerFrame; ++processed)
		{
			Completion completion{};
			{
				std::scoped_lock lock(mutex_);
				if (completions_.empty()) break;
				completion = std::move(completions_.front());
				completions_.erase(completions_.begin());
			}

			if (completion.state && !completion.state->canceled.load(std::memory_order_acquire))
			{
				try
				{
					completion.callback(std::move(completion.payload), completion.exception);
				}
				catch (...)
				{
					// Main Thread側の反映失敗をStreaming Manager自身の寿命問題へ波及させない。
				}
			}

			std::scoped_lock lock(mutex_);
			requests_.erase(completion.requestId);
		}
	}

	std::size_t StreamingManager::GetPendingRequestCount() const
	{
		std::scoped_lock lock(mutex_);
		return requests_.size();
	}

	std::size_t StreamingManager::GetQueuedCompletionCount() const
	{
		std::scoped_lock lock(mutex_);
		return completions_.size();
	}

	JobPriority StreamingManager::ToJobPriority(StreamingPriority priority)
	{
		switch (priority)
		{
		case StreamingPriority::Critical: return JobPriority::Critical;
		case StreamingPriority::High: return JobPriority::High;
		case StreamingPriority::Background: return JobPriority::Low;
		case StreamingPriority::Normal:
		default: return JobPriority::Normal;
		}
	}
} // namespace Ken4lowEngine
