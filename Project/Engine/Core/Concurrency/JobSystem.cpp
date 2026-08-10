#include "JobSystem.h"

#include <algorithm>

namespace Ken4lowEngine
{
	bool JobHandle::HasFailed() const
	{
		if (!state_) return false;
		std::scoped_lock lock(state_->mutex);
		return state_->firstException != nullptr;
	}

	void JobHandle::RethrowIfFailed() const
	{
		if (!state_) return;
		std::exception_ptr exception;
		{
			std::scoped_lock lock(state_->mutex);
			exception = state_->firstException;
		}
		if (exception) std::rethrow_exception(exception);
	}

	JobSystem* JobSystem::GetInstance()
	{
		static JobSystem instance;
		return &instance;
	}

	JobSystem::~JobSystem()
	{
		Finalize();
	}

	void JobSystem::Initialize(std::size_t workerCount)
	{
		if (IsInitialized()) return;

		{
			std::scoped_lock lock(queueMutex_);
			stopping_ = false;
			for (auto& queue : queues_) queue.clear();
		}
		pendingTasks_.store(0, std::memory_order_release);

		const std::size_t resolvedWorkerCount = ResolveWorkerCount(workerCount);
		workers_.reserve(resolvedWorkerCount);
		initialized_.store(true, std::memory_order_release);
		for (std::size_t index = 0; index < resolvedWorkerCount; ++index)
		{
			workers_.emplace_back([this]() { WorkerLoop(); });
		}
	}

	void JobSystem::Finalize()
	{
		if (!IsInitialized()) return;

		{
			std::scoped_lock lock(queueMutex_);
			stopping_ = true;
		}
		queueCv_.notify_all();
		workers_.clear(); // jthreadのdestructorで各Workerの終了を待つ。

		{
			std::scoped_lock lock(queueMutex_);
			for (auto& queue : queues_) queue.clear();
			stopping_ = false;
		}
		pendingTasks_.store(0, std::memory_order_release);
		initialized_.store(false, std::memory_order_release);
		idleCv_.notify_all();
	}

	JobHandle JobSystem::Dispatch(Job job, JobPriority priority)
	{
		if (!job) return {};
		if (!IsInitialized()) Initialize();

		auto state = std::make_shared<JobState>();
		state->remaining.store(1, std::memory_order_release);
		EnqueueTask({ std::move(job), state }, priority);
		return JobHandle(std::move(state));
	}

	JobHandle JobSystem::ParallelFor(
		std::size_t itemCount,
		std::size_t grainSize,
		IndexedJob job,
		JobPriority priority)
	{
		if (!job || itemCount == 0) return {};
		if (!IsInitialized()) Initialize();
		grainSize = (std::max)(std::size_t{ 1 }, grainSize);

		const std::size_t chunkCount = (itemCount + grainSize - 1) / grainSize;
		auto state = std::make_shared<JobState>();
		state->remaining.store(chunkCount, std::memory_order_release);
		auto sharedJob = std::make_shared<IndexedJob>(std::move(job));

		for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
		{
			const std::size_t begin = chunkIndex * grainSize;
			const std::size_t end = (std::min)(itemCount, begin + grainSize);
			EnqueueTask(
				{
					[sharedJob, begin, end]()
					{
						for (std::size_t index = begin; index < end; ++index) (*sharedJob)(index);
					},
					state,
				},
				priority);
		}
		return JobHandle(std::move(state));
	}

	void JobSystem::Wait(const JobHandle& handle) const
	{
		if (!handle.state_) return;
		std::unique_lock lock(handle.state_->mutex);
		handle.state_->completionCv.wait(lock, [&handle]()
			{
				return handle.state_->remaining.load(std::memory_order_acquire) == 0;
			});
	}

	void JobSystem::WaitIdle() const
	{
		std::unique_lock lock(queueMutex_);
		idleCv_.wait(lock, [this]()
			{
				return pendingTasks_.load(std::memory_order_acquire) == 0;
			});
	}

	void JobSystem::WorkerLoop()
	{
		for (;;)
		{
			Task task{};
			{
				std::unique_lock lock(queueMutex_);
				queueCv_.wait(lock, [this]() { return stopping_ || HasQueuedTaskLocked(); });
				if (stopping_ && !HasQueuedTaskLocked()) return;
				task = PopTaskLocked();
			}

			std::exception_ptr exception;
			try
			{
				if (task.job) task.job();
			}
			catch (...)
			{
				exception = std::current_exception();
			}
			CompleteTask(task.state, exception);

			if (pendingTasks_.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				idleCv_.notify_all();
			}
		}
	}

	bool JobSystem::HasQueuedTaskLocked() const
	{
		for (const auto& queue : queues_)
		{
			if (!queue.empty()) return true;
		}
		return false;
	}

	JobSystem::Task JobSystem::PopTaskLocked()
	{
		for (std::size_t priority = queues_.size(); priority-- > 0;)
		{
			auto& queue = queues_[priority];
			if (queue.empty()) continue;
			Task task = std::move(queue.front());
			queue.pop_front();
			return task;
		}
		return {};
	}

	void JobSystem::EnqueueTask(Task task, JobPriority priority)
	{
		const std::size_t priorityIndex = (std::min)(
			static_cast<std::size_t>(priority),
			queues_.size() - 1);
		{
			std::scoped_lock lock(queueMutex_);
			queues_[priorityIndex].push_back(std::move(task));
			pendingTasks_.fetch_add(1, std::memory_order_release);
		}
		queueCv_.notify_one();
	}

	void JobSystem::CompleteTask(const std::shared_ptr<JobState>& state, std::exception_ptr exception)
	{
		if (!state) return;
		if (exception)
		{
			std::scoped_lock lock(state->mutex);
			if (!state->firstException) state->firstException = exception;
		}
		if (state->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
		{
			state->completionCv.notify_all();
		}
	}

	std::size_t JobSystem::ResolveWorkerCount(std::size_t requestedWorkerCount)
	{
		if (requestedWorkerCount > 0) return requestedWorkerCount;
		const unsigned int hardwareThreads = std::thread::hardware_concurrency();
		if (hardwareThreads <= 1) return 1;
		return static_cast<std::size_t>(hardwareThreads - 1);
	}
} // namespace Ken4lowEngine
