#include "JobSystem.h"

#include <algorithm>
#include <queue>
#include <stdexcept>
#include <unordered_map>

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

	JobHandle JobSystem::DispatchAfter(
		const JobHandle& dependency,
		Job job,
		JobPriority priority)
	{
		return DispatchAfter(std::vector<JobHandle>{ dependency }, std::move(job), priority);
	}

	JobHandle JobSystem::DispatchAfter(
		const std::vector<JobHandle>& dependencies,
		Job job,
		JobPriority priority)
	{
		if (!job) return {};
		if (!IsInitialized()) Initialize();

		auto state = std::make_shared<JobState>();
		state->remaining.store(1, std::memory_order_release);
		std::vector<Task> tasks;
		tasks.push_back({ std::move(job), state });
		EnqueueTasksAfterDependencies(std::move(tasks), dependencies, priority);
		return JobHandle(std::move(state));
	}

	JobHandle JobSystem::ParallelFor(
		std::size_t itemCount,
		std::size_t grainSize,
		IndexedJob job,
		JobPriority priority)
	{
		return ParallelForAfter({}, itemCount, grainSize, std::move(job), priority);
	}

	JobHandle JobSystem::ParallelForAfter(
		const std::vector<JobHandle>& dependencies,
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
		std::vector<Task> tasks;
		tasks.reserve(chunkCount);

		for (std::size_t chunkIndex = 0; chunkIndex < chunkCount; ++chunkIndex)
		{
			const std::size_t begin = chunkIndex * grainSize;
			const std::size_t end = (std::min)(itemCount, begin + grainSize);
			tasks.push_back(
				{
					[sharedJob, begin, end]()
					{
						for (std::size_t index = begin; index < end; ++index) (*sharedJob)(index);
					},
					state,
				});
		}

		EnqueueTasksAfterDependencies(std::move(tasks), dependencies, priority);
		return JobHandle(std::move(state));
	}

	JobHandle JobSystem::CreateCompletedHandle()
	{
		auto state = std::make_shared<JobState>();
		state->remaining.store(0, std::memory_order_release); // MainThread systemもJob dependency graph上では完了済みHandleとして表現する。
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

	void JobSystem::EnqueueTasksAfterDependencies(
		std::vector<Task> tasks,
		const std::vector<JobHandle>& dependencies,
		JobPriority priority)
	{
		if (tasks.empty()) return;

		auto batch = std::make_shared<DependencyBatch>();
		batch->tasks = std::move(tasks);
		batch->priority = priority;

		// Dependency registration is closed before release so fast prerequisites cannot enqueue a partial fan-in.
		for (const JobHandle& dependency : dependencies)
		{
			if (!dependency.state_) continue;

			batch->remainingDependencies.fetch_add(1, std::memory_order_acq_rel);
			const bool registered = RegisterCompletionCallback(
				dependency.state_,
				[this, batch]()
				{
					if (batch->remainingDependencies.fetch_sub(1, std::memory_order_acq_rel) == 1)
					{
						TryEnqueueDependencyBatch(batch);
					}
				});

			if (!registered)
			{
				batch->remainingDependencies.fetch_sub(1, std::memory_order_acq_rel);
			}
		}

		batch->registrationComplete.store(true, std::memory_order_release);
		TryEnqueueDependencyBatch(batch);
	}

	void JobSystem::TryEnqueueDependencyBatch(const std::shared_ptr<DependencyBatch>& batch)
	{
		if (!batch) return;
		if (!batch->registrationComplete.load(std::memory_order_acquire)) return;
		if (batch->remainingDependencies.load(std::memory_order_acquire) != 0) return;

		bool expected = false;
		if (!batch->enqueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

		for (Task& task : batch->tasks)
		{
			EnqueueTask(std::move(task), batch->priority);
		}
		batch->tasks.clear();
	}

	bool JobSystem::RegisterCompletionCallback(
		const std::shared_ptr<JobState>& state,
		std::function<void()> callback)
	{
		if (!state || !callback) return false;

		std::scoped_lock lock(state->mutex);
		if (state->remaining.load(std::memory_order_acquire) == 0) return false;
		state->completionCallbacks.push_back(std::move(callback));
		return true;
	}

	void JobSystem::CompleteTask(const std::shared_ptr<JobState>& state, std::exception_ptr exception)
	{
		if (!state) return;

		std::vector<std::function<void()>> completionCallbacks;
		bool completed = false;
		{
			std::scoped_lock lock(state->mutex);
			if (exception && !state->firstException)
			{
				state->firstException = exception;
			}

			if (state->remaining.fetch_sub(1, std::memory_order_acq_rel) == 1)
			{
				completionCallbacks = std::move(state->completionCallbacks);
				completed = true;
			}
		}

		if (!completed) return;
		for (auto& callback : completionCallbacks)
		{
			if (callback) callback();
		}
		state->completionCv.notify_all();
	}

	std::size_t JobSystem::ResolveWorkerCount(std::size_t requestedWorkerCount)
	{
		if (requestedWorkerCount > 0) return requestedWorkerCount;
		const unsigned int hardwareThreads = std::thread::hardware_concurrency();
		if (hardwareThreads <= 1) return 1;
		return static_cast<std::size_t>(hardwareThreads - 1);
	}

	SystemHandle SystemScheduler::AddSystem(
		std::string name,
		SystemJob job,
		std::vector<SystemResourceAccess> accesses,
		SystemExecutionPolicy executionPolicy,
		JobPriority priority)
	{
		if (!job) return {};

		SystemEntry entry{};
		entry.name = std::move(name);
		entry.job = std::move(job);
		entry.accesses = NormalizeAccesses(std::move(accesses));
		entry.executionPolicy = executionPolicy;
		entry.priority = priority;

		const SystemHandle handle{ systems_.size() };
		systems_.push_back(std::move(entry));
		compiled_ = false;
		return handle;
	}

	bool SystemScheduler::AddDependency(SystemHandle before, SystemHandle after)
	{
		if (!IsValidHandle(before) || !IsValidHandle(after) || before == after) return false;
		auto& prerequisites = systems_[after.index].explicitPrerequisites;
		if (std::find(prerequisites.begin(), prerequisites.end(), before) == prerequisites.end())
		{
			prerequisites.push_back(before);
		}
		compiled_ = false;
		return true;
	}

	bool SystemScheduler::Compile()
	{
		compiledOrder_.clear();
		dependencyRecords_.clear();
		stats_ = {};
		stats_.systemCount = systems_.size();

		for (SystemEntry& system : systems_)
		{
			system.compiledPrerequisites.clear();
			if (system.executionPolicy == SystemExecutionPolicy::Worker) ++stats_.workerSystemCount;
			else ++stats_.mainThreadSystemCount;
		}

		for (std::size_t afterIndex = 0; afterIndex < systems_.size(); ++afterIndex)
		{
			const SystemHandle after{ afterIndex };
			for (SystemHandle before : systems_[afterIndex].explicitPrerequisites)
			{
				AddCompiledDependency(before, after, SystemDependencyType::Explicit, 0);
			}
		}

		struct ResourceTracker
		{
			SystemHandle lastWriter{};
			std::vector<SystemHandle> readers;
		};
		std::unordered_map<SystemResourceId, ResourceTracker> resourceTrackers;

		for (std::size_t systemIndex = 0; systemIndex < systems_.size(); ++systemIndex)
		{
			const SystemHandle current{ systemIndex };
			for (const SystemResourceAccess& access : systems_[systemIndex].accesses)
			{
				ResourceTracker& tracker = resourceTrackers[access.resource];
				if (access.access == SystemAccessType::Read)
				{
					if (tracker.lastWriter.IsValid())
					{
						AddCompiledDependency(
							tracker.lastWriter,
							current,
							SystemDependencyType::ReadAfterWrite,
							access.resource);
					}
					tracker.readers.push_back(current);
					continue;
				}

				if (tracker.lastWriter.IsValid())
				{
					if (access.access == SystemAccessType::ReadWrite)
					{
						AddCompiledDependency(
							tracker.lastWriter,
							current,
							SystemDependencyType::ReadAfterWrite,
							access.resource);
					}
					AddCompiledDependency(
						tracker.lastWriter,
						current,
						SystemDependencyType::WriteAfterWrite,
						access.resource);
				}

				for (SystemHandle reader : tracker.readers)
				{
					AddCompiledDependency(
						reader,
						current,
						SystemDependencyType::WriteAfterRead,
						access.resource);
				}
				tracker.readers.clear();
				tracker.lastWriter = current;
			}
		}

		std::vector<std::size_t> indegree(systems_.size(), 0);
		std::vector<std::vector<std::size_t>> outgoing(systems_.size());
		for (std::size_t afterIndex = 0; afterIndex < systems_.size(); ++afterIndex)
		{
			for (SystemHandle before : systems_[afterIndex].compiledPrerequisites)
			{
				if (!IsValidHandle(before)) continue;
				++indegree[afterIndex];
				outgoing[before.index].push_back(afterIndex);
			}
			stats_.dependencyCount += systems_[afterIndex].compiledPrerequisites.size();
		}

		std::priority_queue<std::size_t, std::vector<std::size_t>, std::greater<std::size_t>> ready;
		for (std::size_t index = 0; index < indegree.size(); ++index)
		{
			if (indegree[index] == 0) ready.push(index);
		}

		while (!ready.empty())
		{
			const std::size_t current = ready.top();
			ready.pop();
			compiledOrder_.push_back({ current });
			for (std::size_t dependent : outgoing[current])
			{
				if (--indegree[dependent] == 0) ready.push(dependent);
			}
		}

		compiled_ = compiledOrder_.size() == systems_.size();
		if (!compiled_) compiledOrder_.clear();
		return compiled_;
	}

	void SystemScheduler::ExecuteAndWait(float deltaTime, JobSystem* jobSystem)
	{
		if (!compiled_ && !Compile())
		{
			throw std::logic_error("SystemScheduler dependency graph contains a cycle.");
		}
		if (systems_.empty()) return;

		jobSystem = jobSystem ? jobSystem : JobSystem::GetInstance();
		if (!jobSystem->IsInitialized()) jobSystem->Initialize();

		std::vector<JobHandle> systemJobs(systems_.size());
		std::exception_ptr firstException;
		for (SystemHandle handle : compiledOrder_)
		{
			SystemEntry& system = systems_[handle.index];
			std::vector<JobHandle> prerequisites;
			prerequisites.reserve(system.compiledPrerequisites.size());
			for (SystemHandle prerequisite : system.compiledPrerequisites)
			{
				if (!IsValidHandle(prerequisite)) continue;
				const JobHandle& dependencyJob = systemJobs[prerequisite.index];
				if (dependencyJob.IsValid()) prerequisites.push_back(dependencyJob);
			}

			if (system.executionPolicy == SystemExecutionPolicy::Worker)
			{
				SystemJob job = system.job;
				systemJobs[handle.index] = jobSystem->DispatchAfter(
					prerequisites,
					[job = std::move(job), deltaTime]()
					{
						job(deltaTime);
					},
					system.priority);
				continue;
			}

			// MainThread systemは依存先だけを待ち、無関係なWorker systemとは並行できる余地を残す。
			for (const JobHandle& dependencyJob : prerequisites) jobSystem->Wait(dependencyJob);
			try
			{
				system.job(deltaTime);
			}
			catch (...)
			{
				if (!firstException) firstException = std::current_exception();
			}
			systemJobs[handle.index] = jobSystem->CreateCompletedHandle();
		}

		for (const JobHandle& job : systemJobs)
		{
			if (!job.IsValid()) continue;
			jobSystem->Wait(job);
			if (firstException || !job.HasFailed()) continue;
			try
			{
				job.RethrowIfFailed();
			}
			catch (...)
			{
				firstException = std::current_exception();
			}
		}

		if (firstException) std::rethrow_exception(firstException);
	}

	void SystemScheduler::Reset()
	{
		systems_.clear();
		compiledOrder_.clear();
		dependencyRecords_.clear();
		stats_ = {};
		compiled_ = false;
	}

	std::string_view SystemScheduler::GetSystemName(SystemHandle handle) const
	{
		return IsValidHandle(handle) ? std::string_view{ systems_[handle.index].name } : std::string_view{};
	}

	SystemExecutionPolicy SystemScheduler::GetExecutionPolicy(SystemHandle handle) const
	{
		return IsValidHandle(handle)
			? systems_[handle.index].executionPolicy
			: SystemExecutionPolicy::MainThread;
	}

	bool SystemScheduler::IsValidHandle(SystemHandle handle) const
	{
		return handle.IsValid() && handle.index < systems_.size();
	}

	void SystemScheduler::AddCompiledDependency(
		SystemHandle before,
		SystemHandle after,
		SystemDependencyType type,
		SystemResourceId resource)
	{
		if (!IsValidHandle(before) || !IsValidHandle(after) || before == after) return;

		auto& prerequisites = systems_[after.index].compiledPrerequisites;
		if (std::find(prerequisites.begin(), prerequisites.end(), before) == prerequisites.end())
		{
			prerequisites.push_back(before);
		}

		const auto duplicate = std::find_if(
			dependencyRecords_.begin(),
			dependencyRecords_.end(),
			[before, after, type, resource](const SystemDependencyRecord& record)
			{
				return record.before == before && record.after == after && record.type == type && record.resource == resource;
			});
		if (duplicate != dependencyRecords_.end()) return;

		dependencyRecords_.push_back({ before, after, resource, type });
		switch (type)
		{
		case SystemDependencyType::Explicit: ++stats_.explicitDependencyCount; break;
		case SystemDependencyType::ReadAfterWrite: ++stats_.rawHazardCount; break;
		case SystemDependencyType::WriteAfterRead: ++stats_.warHazardCount; break;
		case SystemDependencyType::WriteAfterWrite: ++stats_.wawHazardCount; break;
		}
	}

	std::vector<SystemResourceAccess> SystemScheduler::NormalizeAccesses(std::vector<SystemResourceAccess> accesses)
	{
		std::vector<SystemResourceAccess> normalized;
		normalized.reserve(accesses.size());
		for (const SystemResourceAccess& access : accesses)
		{
			auto found = std::find_if(
				normalized.begin(),
				normalized.end(),
				[access](const SystemResourceAccess& existing)
				{
					return existing.resource == access.resource;
				});
			if (found == normalized.end())
			{
				normalized.push_back(access);
				continue;
			}

			if (found->access == access.access) continue;
			found->access = SystemAccessType::ReadWrite; // 同一SystemのRead+Write宣言は一つのReadWrite ownershipへ正規化する。
		}
		return normalized;
	}
} // namespace Ken4lowEngine
