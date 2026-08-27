#include "JobSystem.h"

#include <algorithm>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace Ken4lowEngine;

namespace
{
	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	bool HasDependency(
		const SystemScheduler& scheduler,
		SystemHandle before,
		SystemHandle after,
		SystemDependencyType type)
	{
		const auto& dependencies = scheduler.GetDependencies();
		return std::any_of(
			dependencies.begin(),
			dependencies.end(),
			[before, after, type](const SystemDependencyRecord& dependency)
			{
				return dependency.before == before && dependency.after == after && dependency.type == type;
			});
	}

	void TestReadWriteHazards()
	{
		SystemScheduler scheduler;
		const SystemHandle readerA = scheduler.AddSystem(
			"ReaderA", [](float) {}, { { 1, SystemAccessType::Read } });
		const SystemHandle readerB = scheduler.AddSystem(
			"ReaderB", [](float) {}, { { 1, SystemAccessType::Read } });
		const SystemHandle writer = scheduler.AddSystem(
			"Writer", [](float) {}, { { 1, SystemAccessType::Write } });
		const SystemHandle readerAfterWrite = scheduler.AddSystem(
			"ReaderAfterWrite", [](float) {}, { { 1, SystemAccessType::Read } });

		Require(scheduler.Compile(), "resource hazard schedule should compile");
		const SystemScheduleStats& stats = scheduler.GetStats();
		Require(stats.systemCount == 4, "unexpected system count");
		Require(stats.dependencyCount == 3, "read/read must not introduce a dependency");
		Require(stats.warHazardCount == 2, "writer must wait for both prior readers");
		Require(stats.rawHazardCount == 1, "reader after write must depend on writer");
		Require(HasDependency(scheduler, readerA, writer, SystemDependencyType::WriteAfterRead), "missing WAR dependency A -> writer");
		Require(HasDependency(scheduler, readerB, writer, SystemDependencyType::WriteAfterRead), "missing WAR dependency B -> writer");
		Require(HasDependency(scheduler, writer, readerAfterWrite, SystemDependencyType::ReadAfterWrite), "missing RAW dependency writer -> reader");
	}

	void TestMainAndWorkerExecution(JobSystem& jobSystem)
	{
		SystemScheduler scheduler;
		std::mutex eventMutex;
		std::vector<std::string> events;
		int value = 0;
		const std::thread::id callerThread = std::this_thread::get_id();
		std::thread::id actorThread;
		std::thread::id physicsThread;
		std::thread::id postThread;

		scheduler.AddSystem(
			"Actor",
			[&](float)
			{
				actorThread = std::this_thread::get_id();
				value = 10;
				std::scoped_lock lock(eventMutex);
				events.push_back("Actor");
			},
			{ { 10, SystemAccessType::Write } },
			SystemExecutionPolicy::MainThread);

		scheduler.AddSystem(
			"Physics",
			[&](float)
			{
				physicsThread = std::this_thread::get_id();
				Require(value == 10, "worker observed actor state before prerequisite completed");
				value = 11;
				std::scoped_lock lock(eventMutex);
				events.push_back("Physics");
			},
			{
				{ 10, SystemAccessType::Read },
				{ 20, SystemAccessType::Write },
			},
			SystemExecutionPolicy::Worker);

		scheduler.AddSystem(
			"PostPhysics",
			[&](float)
			{
				postThread = std::this_thread::get_id();
				Require(value == 11, "main thread post phase observed incomplete worker state");
				std::scoped_lock lock(eventMutex);
				events.push_back("PostPhysics");
			},
			{ { 20, SystemAccessType::Read } },
			SystemExecutionPolicy::MainThread);

		scheduler.ExecuteAndWait(1.0f / 60.0f, &jobSystem);

		// Thread affinity is explicit: only Worker policy may leave the caller thread.
		Require(actorThread == callerThread, "main-thread actor system moved to a worker");
		Require(postThread == callerThread, "main-thread post system moved to a worker");
		Require(physicsThread != callerThread, "worker physics system did not use the worker pool");
		Require(events == std::vector<std::string>{ "Actor", "Physics", "PostPhysics" }, "system execution order was not preserved");
	}

	void TestCycleDetection()
	{
		SystemScheduler scheduler;
		const SystemHandle a = scheduler.AddSystem("A", [](float) {});
		const SystemHandle b = scheduler.AddSystem("B", [](float) {});
		Require(scheduler.AddDependency(a, b), "failed to add A -> B");
		Require(scheduler.AddDependency(b, a), "failed to add B -> A");
		Require(!scheduler.Compile(), "explicit dependency cycle must fail compilation");
	}

	void TestFailureStillReleasesOrderingDependent(JobSystem& jobSystem)
	{
		SystemScheduler scheduler;
		bool dependentRan = false;
		scheduler.AddSystem(
			"FailingWorker",
			[](float)
			{
				throw std::runtime_error("expected scheduler failure");
			},
			{ { 30, SystemAccessType::Write } },
			SystemExecutionPolicy::Worker);
		scheduler.AddSystem(
			"OrderingDependent",
			[&](float)
			{
				dependentRan = true;
			},
			{ { 30, SystemAccessType::Read } },
			SystemExecutionPolicy::MainThread);

		bool threw = false;
		try
		{
			scheduler.ExecuteAndWait(0.016f, &jobSystem);
		}
		catch (const std::runtime_error&)
		{
			threw = true;
		}
		Require(dependentRan, "failed prerequisite must still release ordering-only dependent");
		Require(threw, "worker failure must be rethrown at the schedule frame boundary");
	}
}

int main()
{
	try
	{
		JobSystem* jobSystem = JobSystem::GetInstance();
		jobSystem->Finalize();
		jobSystem->Initialize(2);

		TestReadWriteHazards();
		TestMainAndWorkerExecution(*jobSystem);
		TestCycleDetection();
		TestFailureStillReleasesOrderingDependent(*jobSystem);

		jobSystem->Finalize();
		std::cout << "System Scheduler runtime tests passed\n";
		return 0;
	}
	catch (const std::exception& exception)
	{
		JobSystem::GetInstance()->Finalize();
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
