#include "JobSystem.h"

#include <array>
#include <atomic>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
	using Ken4lowEngine::JobHandle;
	using Ken4lowEngine::JobSystem;

	void Require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	void TestLinearChain(JobSystem& jobs)
	{
		std::atomic_int stage{ 0 };
		const JobHandle first = jobs.Dispatch([&stage]() { stage.store(1, std::memory_order_release); });
		const JobHandle second = jobs.DispatchAfter(first, [&stage]()
			{
				if (stage.load(std::memory_order_acquire) != 1)
				{
					throw std::runtime_error("linear dependency B ran before A");
				}
				stage.store(2, std::memory_order_release);
			});
		const JobHandle third = jobs.DispatchAfter(second, [&stage]()
			{
				if (stage.load(std::memory_order_acquire) != 2)
				{
					throw std::runtime_error("linear dependency C ran before B");
				}
				stage.store(3, std::memory_order_release);
			});

		jobs.Wait(third);
		second.RethrowIfFailed();
		third.RethrowIfFailed();
		Require(stage.load(std::memory_order_acquire) == 3, "linear dependency chain did not finish");
	}

	void TestFanIn(JobSystem& jobs)
	{
		std::promise<void> releaseSecond;
		std::shared_future<void> secondSignal = releaseSecond.get_future().share();
		std::atomic_bool firstDone{ false };
		std::atomic_bool secondDone{ false };
		std::atomic_bool fanInRan{ false };

		const JobHandle first = jobs.Dispatch([&firstDone]() { firstDone.store(true, std::memory_order_release); });
		const JobHandle second = jobs.Dispatch([secondSignal, &secondDone]()
			{
				secondSignal.wait();
				secondDone.store(true, std::memory_order_release);
			});
		const JobHandle fanIn = jobs.DispatchAfter(std::vector<JobHandle>{ first, second }, [&]()
			{
				if (!firstDone.load(std::memory_order_acquire) || !secondDone.load(std::memory_order_acquire))
				{
					throw std::runtime_error("fan-in job ran before every prerequisite completed");
				}
				fanInRan.store(true, std::memory_order_release);
			});

		jobs.Wait(first);
		Require(!fanIn.IsComplete(), "fan-in job completed while one prerequisite was blocked");
		releaseSecond.set_value();
		jobs.Wait(fanIn);
		fanIn.RethrowIfFailed();
		Require(fanInRan.load(std::memory_order_acquire), "fan-in job never ran");
	}

	void TestFanOut(JobSystem& jobs)
	{
		std::promise<void> releaseParent;
		std::shared_future<void> parentSignal = releaseParent.get_future().share();
		std::atomic_int children{ 0 };
		const JobHandle parent = jobs.Dispatch([parentSignal]() { parentSignal.wait(); });
		const JobHandle left = jobs.DispatchAfter(parent, [&children]() { children.fetch_add(1, std::memory_order_acq_rel); });
		const JobHandle right = jobs.DispatchAfter(parent, [&children]() { children.fetch_add(1, std::memory_order_acq_rel); });

		Require(!left.IsComplete() && !right.IsComplete(), "fan-out child ran before the parent was released");
		releaseParent.set_value();
		jobs.Wait(left);
		jobs.Wait(right);
		Require(children.load(std::memory_order_acquire) == 2, "fan-out did not release every child");
	}

	void TestCompletedAndInvalidDependencies(JobSystem& jobs)
	{
		std::atomic_int value{ 0 };
		const JobHandle completed = jobs.Dispatch([&value]() { value.fetch_add(1, std::memory_order_acq_rel); });
		jobs.Wait(completed);

		const JobHandle afterCompleted = jobs.DispatchAfter(completed, [&value]() { value.fetch_add(1, std::memory_order_acq_rel); });
		const JobHandle afterInvalid = jobs.DispatchAfter(JobHandle{}, [&value]() { value.fetch_add(1, std::memory_order_acq_rel); });
		jobs.Wait(afterCompleted);
		jobs.Wait(afterInvalid);
		Require(value.load(std::memory_order_acquire) == 3, "completed or invalid dependencies did not release immediately");
	}

	void TestParallelForAfter(JobSystem& jobs)
	{
		std::promise<void> releaseParent;
		std::shared_future<void> parentSignal = releaseParent.get_future().share();
		std::array<std::atomic_int, 64> visits{};
		const JobHandle parent = jobs.Dispatch([parentSignal]() { parentSignal.wait(); });
		const JobHandle parallel = jobs.ParallelForAfter(
			std::vector<JobHandle>{ parent },
			visits.size(),
			7,
			[&visits](std::size_t index) { visits[index].fetch_add(1, std::memory_order_acq_rel); });

		for (const auto& visit : visits)
		{
			Require(visit.load(std::memory_order_acquire) == 0, "parallel batch ran before its dependency");
		}
		releaseParent.set_value();
		jobs.Wait(parallel);

		// Every chunk shares one JobState, so the returned handle completes only after all indices finish.
		for (const auto& visit : visits)
		{
			Require(visit.load(std::memory_order_acquire) == 1, "parallel dependency batch missed or repeated an index");
		}
	}

	void TestFailedDependencyStillReleases(JobSystem& jobs)
	{
		std::atomic_bool dependentRan{ false };
		const JobHandle failing = jobs.Dispatch([]() { throw std::runtime_error("expected prerequisite failure"); });
		const JobHandle dependent = jobs.DispatchAfter(failing, [&dependentRan]() { dependentRan.store(true, std::memory_order_release); });

		jobs.Wait(dependent);
		Require(failing.HasFailed(), "failed prerequisite did not retain its exception");
		Require(dependentRan.load(std::memory_order_acquire), "failed prerequisite blocked an ordering-only dependent");
	}
}

int main()
{
	JobSystem* jobs = JobSystem::GetInstance();
	try
	{
		jobs->Initialize(2);
		TestLinearChain(*jobs);
		TestFanIn(*jobs);
		TestFanOut(*jobs);
		TestCompletedAndInvalidDependencies(*jobs);
		TestParallelForAfter(*jobs);
		TestFailedDependencyStillReleases(*jobs);
		jobs->WaitIdle();
		jobs->Finalize();
		std::cout << "Phase 10.1 Job Dependency runtime tests passed.\n";
		return 0;
	}
	catch (const std::exception& exception)
	{
		jobs->Finalize();
		std::cerr << "Phase 10.1 Job Dependency runtime test failed: " << exception.what() << '\n';
		return 1;
	}
}
