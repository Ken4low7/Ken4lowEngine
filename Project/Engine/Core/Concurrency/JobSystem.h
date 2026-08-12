#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	enum class JobPriority : uint8_t
	{
		Low = 0,
		Normal,
		High,
		Critical,
		Count,
	};

	struct JobState
	{
		std::atomic_size_t remaining{ 0 };
		std::mutex mutex;
		std::condition_variable completionCv;
		std::exception_ptr firstException;
		std::vector<std::function<void()>> completionCallbacks;
	};

	class JobHandle
	{
	public:
		JobHandle() = default; // 空Jobや投入失敗を表す無効Handleとして使用する。
		[[nodiscard]] bool IsValid() const { return state_ != nullptr; }
		[[nodiscard]] bool IsComplete() const { return !state_ || state_->remaining.load(std::memory_order_acquire) == 0; }
		[[nodiscard]] bool HasFailed() const;
		void RethrowIfFailed() const;

	private:
		explicit JobHandle(std::shared_ptr<JobState> state) : state_(std::move(state)) {}
		std::shared_ptr<JobState> state_;
		friend class JobSystem;
	};

	/// <summary>
	/// Engine全体で共有する固定Worker Thread Pool。
	/// GPU APIやEditor UIはWorkerから触らず、CPU側の独立処理だけを投入する。
	/// </summary>
	class JobSystem
	{
	public:
		using Job = std::function<void()>;
		using IndexedJob = std::function<void(std::size_t)>;

		static JobSystem* GetInstance();

		void Initialize(std::size_t workerCount = 0);
		void Finalize();

		JobHandle Dispatch(Job job, JobPriority priority = JobPriority::Normal);
		JobHandle DispatchAfter(
			const JobHandle& dependency,
			Job job,
			JobPriority priority = JobPriority::Normal);
		JobHandle DispatchAfter(
			const std::vector<JobHandle>& dependencies,
			Job job,
			JobPriority priority = JobPriority::Normal);
		JobHandle ParallelFor(
			std::size_t itemCount,
			std::size_t grainSize,
			IndexedJob job,
			JobPriority priority = JobPriority::Normal);
		JobHandle ParallelForAfter(
			const std::vector<JobHandle>& dependencies,
			std::size_t itemCount,
			std::size_t grainSize,
			IndexedJob job,
			JobPriority priority = JobPriority::Normal);
		JobHandle CreateCompletedHandle();

		void Wait(const JobHandle& handle) const;
		void WaitIdle() const;

		[[nodiscard]] bool IsInitialized() const { return initialized_.load(std::memory_order_acquire); }
		[[nodiscard]] std::size_t GetWorkerCount() const { return workers_.size(); }
		[[nodiscard]] std::size_t GetPendingJobCount() const { return pendingTasks_.load(std::memory_order_acquire); }

	private:
		struct Task
		{
			Job job;
			std::shared_ptr<JobState> state;
		};

		struct DependencyBatch
		{
			std::vector<Task> tasks;
			JobPriority priority = JobPriority::Normal;
			std::atomic_size_t remainingDependencies{ 0 };
			std::atomic_bool registrationComplete{ false };
			std::atomic_bool enqueued{ false };
		};

		JobSystem() = default;
		~JobSystem();
		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		void WorkerLoop();
		bool HasQueuedTaskLocked() const;
		Task PopTaskLocked();
		void EnqueueTask(Task task, JobPriority priority);
		void EnqueueTasksAfterDependencies(
			std::vector<Task> tasks,
			const std::vector<JobHandle>& dependencies,
			JobPriority priority);
		void TryEnqueueDependencyBatch(const std::shared_ptr<DependencyBatch>& batch);
		static bool RegisterCompletionCallback(
			const std::shared_ptr<JobState>& state,
			std::function<void()> callback);
		static void CompleteTask(const std::shared_ptr<JobState>& state, std::exception_ptr exception);
		static std::size_t ResolveWorkerCount(std::size_t requestedWorkerCount);

		mutable std::mutex queueMutex_;
		mutable std::condition_variable queueCv_;
		mutable std::condition_variable idleCv_;
		std::array<std::deque<Task>, static_cast<std::size_t>(JobPriority::Count)> queues_{};
		std::vector<std::jthread> workers_;
		std::atomic_size_t pendingTasks_{ 0 };
		std::atomic_bool initialized_{ false };
		bool stopping_ = false;
	};

	using SystemResourceId = uint32_t;

	enum class SystemAccessType : uint8_t
	{
		Read,
		Write,
		ReadWrite,
	};

	enum class SystemExecutionPolicy : uint8_t
	{
		MainThread,
		Worker,
	};

	enum class SystemDependencyType : uint8_t
	{
		Explicit,
		ReadAfterWrite,
		WriteAfterRead,
		WriteAfterWrite,
	};

	struct SystemHandle
	{
		static constexpr std::size_t InvalidIndex = (std::numeric_limits<std::size_t>::max)();
		std::size_t index = InvalidIndex;

		[[nodiscard]] bool IsValid() const { return index != InvalidIndex; }
		friend bool operator==(const SystemHandle&, const SystemHandle&) = default;
	};

	struct SystemResourceAccess
	{
		SystemResourceId resource = 0;
		SystemAccessType access = SystemAccessType::Read;
	};

	struct SystemDependencyRecord
	{
		SystemHandle before{};
		SystemHandle after{};
		SystemResourceId resource = 0;
		SystemDependencyType type = SystemDependencyType::Explicit;
	};

	struct SystemScheduleStats
	{
		std::size_t systemCount = 0;
		std::size_t dependencyCount = 0;
		std::size_t explicitDependencyCount = 0;
		std::size_t rawHazardCount = 0;
		std::size_t warHazardCount = 0;
		std::size_t wawHazardCount = 0;
		std::size_t mainThreadSystemCount = 0;
		std::size_t workerSystemCount = 0;
	};

	/// <summary>
	/// World/Systemのread-write宣言から依存DAGを構築し、既存JobSystemへ実行を委譲する。
	/// MainThread指定はthread affinityを維持し、Worker指定だけが固定Worker Poolへ投入される。
	/// </summary>
	class SystemScheduler
	{
	public:
		using SystemJob = std::function<void(float)>;

		SystemHandle AddSystem(
			std::string name,
			SystemJob job,
			std::vector<SystemResourceAccess> accesses = {},
			SystemExecutionPolicy executionPolicy = SystemExecutionPolicy::MainThread,
			JobPriority priority = JobPriority::Normal);
		bool AddDependency(SystemHandle before, SystemHandle after);

		bool Compile();
		void ExecuteAndWait(float deltaTime, JobSystem* jobSystem = nullptr);
		void Reset();

		[[nodiscard]] bool IsCompiled() const { return compiled_; }
		[[nodiscard]] const std::vector<SystemHandle>& GetCompiledOrder() const { return compiledOrder_; }
		[[nodiscard]] const std::vector<SystemDependencyRecord>& GetDependencies() const { return dependencyRecords_; }
		[[nodiscard]] const SystemScheduleStats& GetStats() const { return stats_; }
		[[nodiscard]] std::string_view GetSystemName(SystemHandle handle) const;
		[[nodiscard]] SystemExecutionPolicy GetExecutionPolicy(SystemHandle handle) const;

	private:
		struct SystemEntry
		{
			std::string name;
			SystemJob job;
			std::vector<SystemResourceAccess> accesses;
			SystemExecutionPolicy executionPolicy = SystemExecutionPolicy::MainThread;
			JobPriority priority = JobPriority::Normal;
			std::vector<SystemHandle> explicitPrerequisites;
			std::vector<SystemHandle> compiledPrerequisites;
		};

		bool IsValidHandle(SystemHandle handle) const;
		void AddCompiledDependency(
			SystemHandle before,
			SystemHandle after,
			SystemDependencyType type,
			SystemResourceId resource);
		static std::vector<SystemResourceAccess> NormalizeAccesses(std::vector<SystemResourceAccess> accesses);

		std::vector<SystemEntry> systems_;
		std::vector<SystemHandle> compiledOrder_;
		std::vector<SystemDependencyRecord> dependencyRecords_;
		SystemScheduleStats stats_{};
		bool compiled_ = false;
	};
} // namespace Ken4lowEngine
