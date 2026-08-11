#include "RenderGraph.h"

#include <Engine/Core/Memory/FrameMemory.h>

#include <algorithm>
#include <memory_resource>

namespace Ken4lowEngine
{
	void RenderGraph::Reset()
	{
		resources_.clear();
		passes_.clear();
		compiledOrder_.clear();
		dependencyRecords_.clear();
		barrierPlan_.clear();
		compileStats_ = {};
		compiled_ = false;
	}

	RenderGraph::ResourceHandle RenderGraph::CreateResource(std::string name, bool imported)
	{
		return CreateResource(
			std::move(name),
			imported,
			ResourceState::Unknown,
			ResourceState::Unknown);
	}

	RenderGraph::ResourceHandle RenderGraph::CreateResource(
		std::string name,
		bool imported,
		ResourceState initialState,
		ResourceState finalState)
	{
		ResourceNode node{};
		node.name = std::move(name);
		node.lifetime.imported = imported;
		node.initialState = initialState;
		node.finalState = finalState;
		resources_.push_back(std::move(node));
		compiled_ = false;
		return { static_cast<uint32_t>(resources_.size() - 1) };
	}

	RenderGraph::PassHandle RenderGraph::AddPass(
		std::string name,
		std::vector<ResourceAccess> accesses,
		ExecuteCallback execute)
	{
		PassNode node{};
		node.name = std::move(name);
		node.accesses = std::move(accesses);
		node.execute = std::move(execute);
		passes_.push_back(std::move(node));
		compiled_ = false;
		return { static_cast<uint32_t>(passes_.size() - 1) };
	}

	RenderGraph::PassHandle RenderGraph::AddPass(
		std::string name,
		std::vector<ResourceHandle> reads,
		std::vector<ResourceHandle> writes,
		ExecuteCallback execute)
	{
		std::vector<ResourceAccess> accesses;
		accesses.reserve(reads.size() + writes.size());
		for (ResourceHandle handle : reads)
		{
			accesses.push_back({ handle, AccessType::Read, ResourceState::Unknown });
		}
		for (ResourceHandle handle : writes)
		{
			accesses.push_back({ handle, AccessType::Write, ResourceState::Unknown });
		}
		return AddPass(std::move(name), std::move(accesses), std::move(execute));
	}

	RenderGraph::PassHandle RenderGraph::AddPass(
		std::string name,
		std::initializer_list<ResourceHandle> reads,
		std::initializer_list<ResourceHandle> writes,
		ExecuteCallback execute)
	{
		return AddPass(
			std::move(name),
			std::vector<ResourceHandle>(reads),
			std::vector<ResourceHandle>(writes),
			std::move(execute));
	}

	bool RenderGraph::AddDependency(PassHandle before, PassHandle after)
	{
		if (!before.IsValid() || !after.IsValid() ||
			before.id >= passes_.size() || after.id >= passes_.size() || before == after)
		{
			return false;
		}

		auto& dependencies = passes_[after.id].explicitDependencies;
		if (std::find(dependencies.begin(), dependencies.end(), before.id) == dependencies.end())
		{
			dependencies.push_back(before.id);
		}
		compiled_ = false;
		return true;
	}

	bool RenderGraph::Compile(std::string* outError)
	{
		if (outError) outError->clear();
		compiledOrder_.clear();
		dependencyRecords_.clear();
		barrierPlan_.clear();
		compileStats_ = {};
		compileStats_.passCount = passes_.size();
		compileStats_.resourceCount = resources_.size();
		for (ResourceNode& resource : resources_)
		{
			resource.lifetime.firstPass = (std::numeric_limits<std::size_t>::max)();
			resource.lifetime.lastPass = 0;
			if (!resource.lifetime.imported) ++compileStats_.transientResourceCount;
		}

		for (const PassNode& pass : passes_)
		{
			for (const ResourceAccess& access : pass.accesses)
			{
				if (!ValidateResourceHandle(access.resource))
				{
					if (outError) *outError = "Render GraphのResource Access Handleが無効です: " + pass.name;
					return false;
				}
			}
		}

		std::pmr::memory_resource* scratch = FrameMemory::GetInstance()->GetMemoryResource();
		std::pmr::vector<uint32_t> indegree(scratch);
		indegree.assign(passes_.size(), 0u);
		std::vector<std::vector<uint32_t>> adjacency(passes_.size());

		auto addEdge = [&adjacency, &indegree, this](uint32_t before, uint32_t after)
			{
				if (before == after || before >= passes_.size() || after >= passes_.size()) return false;
				auto& edges = adjacency[before];
				if (std::find(edges.begin(), edges.end(), after) != edges.end()) return false;
				edges.push_back(after);
				++indegree[after];
				++compileStats_.dependencyCount;
				return true;
			};

		auto addDependencyRecord = [this, &addEdge](uint32_t before, uint32_t after, ResourceHandle resource, HazardType hazard)
			{
				if (before == after || before >= passes_.size() || after >= passes_.size()) return;
				const auto duplicate = std::find_if(
					dependencyRecords_.begin(), dependencyRecords_.end(),
					[before, after, resource, hazard](const DependencyRecord& record)
					{
						return record.before.id == before && record.after.id == after &&
							record.resource == resource && record.hazard == hazard;
					});
				if (duplicate != dependencyRecords_.end()) return;

				dependencyRecords_.push_back({ PassHandle{ before }, PassHandle{ after }, resource, hazard });
				switch (hazard)
				{
				case HazardType::ReadAfterWrite:
					++compileStats_.rawHazardCount;
					break;
				case HazardType::WriteAfterRead:
					++compileStats_.warHazardCount;
					break;
				case HazardType::WriteAfterWrite:
					++compileStats_.wawHazardCount;
					break;
				default:
					break;
				}
				addEdge(before, after);
			};

		std::pmr::vector<int32_t> lastWriter(scratch);
		lastWriter.assign(resources_.size(), -1);
		std::vector<std::vector<uint32_t>> activeReaders(resources_.size());
		std::pmr::vector<uint8_t> accessMasks(scratch);
		accessMasks.assign(resources_.size(), uint8_t{ 0 });

		for (uint32_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			const PassNode& pass = passes_[passIndex];
			for (uint32_t dependency : pass.explicitDependencies)
			{
				addDependencyRecord(dependency, passIndex, {}, HazardType::Explicit);
			}

			std::fill(accessMasks.begin(), accessMasks.end(), uint8_t{ 0 });
			for (const ResourceAccess& access : pass.accesses)
			{
				ResourceNode& resource = resources_[access.resource.id];
				resource.lifetime.firstPass = (std::min)(resource.lifetime.firstPass, static_cast<std::size_t>(passIndex));
				resource.lifetime.lastPass = (std::max)(resource.lifetime.lastPass, static_cast<std::size_t>(passIndex));
				uint8_t& mask = accessMasks[access.resource.id];
				if (access.access == AccessType::Read || access.access == AccessType::ReadWrite) mask |= uint8_t{ 0x1 };
				if (access.access == AccessType::Write || access.access == AccessType::ReadWrite) mask |= uint8_t{ 0x2 };
			}

			// Read/read access remains parallel; only RAW, WAR and WAW hazards generate ordering edges.
			for (std::size_t resourceIndex = 0; resourceIndex < accessMasks.size(); ++resourceIndex)
			{
				const uint8_t mask = accessMasks[resourceIndex];
				if (mask == uint8_t{ 0 }) continue;
				const bool reads = (mask & uint8_t{ 0x1 }) != uint8_t{ 0 };
				const bool writes = (mask & uint8_t{ 0x2 }) != uint8_t{ 0 };
				const ResourceHandle handle{ static_cast<uint32_t>(resourceIndex) };

				if (reads && lastWriter[resourceIndex] >= 0)
				{
					addDependencyRecord(
						static_cast<uint32_t>(lastWriter[resourceIndex]), passIndex, handle, HazardType::ReadAfterWrite);
				}

				if (writes)
				{
					if (lastWriter[resourceIndex] >= 0)
					{
						addDependencyRecord(
							static_cast<uint32_t>(lastWriter[resourceIndex]), passIndex, handle, HazardType::WriteAfterWrite);
					}
					for (uint32_t reader : activeReaders[resourceIndex])
					{
						addDependencyRecord(reader, passIndex, handle, HazardType::WriteAfterRead);
					}
					activeReaders[resourceIndex].clear();
					lastWriter[resourceIndex] = static_cast<int32_t>(passIndex);
				}
				else if (reads)
				{
					activeReaders[resourceIndex].push_back(passIndex);
				}
			}
		}

		std::pmr::vector<uint32_t> ready(scratch);
		ready.reserve(passes_.size());
		for (uint32_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			if (indegree[passIndex] == 0) ready.push_back(passIndex);
		}

		compiledOrder_.reserve(passes_.size());
		std::size_t readyCursor = 0;
		while (readyCursor < ready.size())
		{
			const uint32_t passIndex = ready[readyCursor++];
			compiledOrder_.push_back(passIndex);
			for (uint32_t dependent : adjacency[passIndex])
			{
				if (--indegree[dependent] == 0) ready.push_back(dependent);
			}
		}

		if (compiledOrder_.size() != passes_.size())
		{
			compiledOrder_.clear();
			if (outError) *outError = "Render Graphに循環依存があります。";
			compiled_ = false;
			return false;
		}

		if (!BuildBarrierPlan(outError))
		{
			compiledOrder_.clear();
			compiled_ = false;
			return false;
		}

		compiled_ = true;
		return true;
	}

	bool RenderGraph::BuildBarrierPlan(std::string* outError)
	{
		barrierPlan_.clear();
		compileStats_.transitionBarrierCount = 0;
		compileStats_.uavBarrierCount = 0;
		compileStats_.unknownStateAccessCount = 0;

		std::vector<ResourceState> currentStates;
		currentStates.reserve(resources_.size());
		for (const ResourceNode& resource : resources_)
		{
			currentStates.push_back(resource.initialState);
		}

		std::vector<ResourceState> previousAccessStates(resources_.size(), ResourceState::Unknown);
		std::vector<uint8_t> previousAccessMasks(resources_.size(), uint8_t{ 0 });
		std::vector<ResourceState> requestedStates(resources_.size(), ResourceState::Unknown);
		std::vector<uint8_t> mergedAccessMasks(resources_.size(), uint8_t{ 0 });

		// Barrier planning stays D3D12-independent so manually managed resources can migrate without double transitions.
		for (uint32_t passIndex : compiledOrder_)
		{
			if (passIndex >= passes_.size()) continue;
			std::fill(requestedStates.begin(), requestedStates.end(), ResourceState::Unknown);
			std::fill(mergedAccessMasks.begin(), mergedAccessMasks.end(), uint8_t{ 0 });

			const PassNode& pass = passes_[passIndex];
			for (const ResourceAccess& access : pass.accesses)
			{
				const std::size_t resourceIndex = access.resource.id;
				uint8_t& mask = mergedAccessMasks[resourceIndex];
				if (access.access == AccessType::Read || access.access == AccessType::ReadWrite) mask |= uint8_t{ 0x1 };
				if (access.access == AccessType::Write || access.access == AccessType::ReadWrite) mask |= uint8_t{ 0x2 };

				if (access.state == ResourceState::Unknown)
				{
					++compileStats_.unknownStateAccessCount;
					continue;
				}

				ResourceState& requestedState = requestedStates[resourceIndex];
				if (requestedState == ResourceState::Unknown)
				{
					requestedState = access.state;
				}
				else if (requestedState != access.state)
				{
					if (outError)
					{
						*outError = "Render Graphの同一Pass内でResource Stateが競合しています: " + pass.name +
							" / " + resources_[resourceIndex].name;
					}
					return false;
				}
			}

			for (std::size_t resourceIndex = 0; resourceIndex < resources_.size(); ++resourceIndex)
			{
				const uint8_t accessMask = mergedAccessMasks[resourceIndex];
				if (accessMask == uint8_t{ 0 }) continue;

				const ResourceHandle resourceHandle{ static_cast<uint32_t>(resourceIndex) };
				const ResourceState requestedState = requestedStates[resourceIndex];
				if (requestedState == ResourceState::Unknown)
				{
					currentStates[resourceIndex] = ResourceState::Unknown;
					previousAccessStates[resourceIndex] = ResourceState::Unknown;
					previousAccessMasks[resourceIndex] = accessMask;
					continue;
				}

				const ResourceState currentState = currentStates[resourceIndex];
				if (currentState != ResourceState::Unknown && currentState != requestedState)
				{
					barrierPlan_.push_back({
						resourceHandle,
						PassHandle{ passIndex },
						BarrierType::Transition,
						BarrierPlacement::BeforePass,
						currentState,
						requestedState,
					});
					++compileStats_.transitionBarrierCount;
				}

				const bool previousWrites = (previousAccessMasks[resourceIndex] & uint8_t{ 0x2 }) != uint8_t{ 0 };
				const bool currentWrites = (accessMask & uint8_t{ 0x2 }) != uint8_t{ 0 };
				if (requestedState == ResourceState::UnorderedAccess &&
					previousAccessStates[resourceIndex] == ResourceState::UnorderedAccess &&
					(previousWrites || currentWrites))
				{
					barrierPlan_.push_back({
						resourceHandle,
						PassHandle{ passIndex },
						BarrierType::UnorderedAccess,
						BarrierPlacement::BeforePass,
						ResourceState::UnorderedAccess,
						ResourceState::UnorderedAccess,
					});
					++compileStats_.uavBarrierCount;
				}

				currentStates[resourceIndex] = requestedState;
				previousAccessStates[resourceIndex] = requestedState;
				previousAccessMasks[resourceIndex] = accessMask;
			}
		}

		for (std::size_t resourceIndex = 0; resourceIndex < resources_.size(); ++resourceIndex)
		{
			const ResourceState finalState = resources_[resourceIndex].finalState;
			const ResourceState currentState = currentStates[resourceIndex];
			if (finalState == ResourceState::Unknown || currentState == ResourceState::Unknown || finalState == currentState)
			{
				continue;
			}

			barrierPlan_.push_back({
				ResourceHandle{ static_cast<uint32_t>(resourceIndex) },
				{},
				BarrierType::Transition,
				BarrierPlacement::AfterGraph,
				currentState,
				finalState,
			});
			++compileStats_.transitionBarrierCount;
		}

		return true;
	}

	bool RenderGraph::Execute(std::string* outError)
	{
		return Execute(BarrierCallback{}, outError);
	}

	bool RenderGraph::Execute(const BarrierCallback& barrierCallback, std::string* outError)
	{
		if (!compiled_ && !Compile(outError)) return false;
		for (uint32_t passIndex : compiledOrder_)
		{
			if (passIndex >= passes_.size()) continue;
			if (barrierCallback)
			{
				for (const BarrierRecord& barrier : barrierPlan_)
				{
					if (barrier.placement == BarrierPlacement::BeforePass && barrier.pass.id == passIndex)
					{
						barrierCallback(barrier);
					}
				}
			}

			const ExecuteCallback& callback = passes_[passIndex].execute;
			if (callback) callback();
		}

		if (barrierCallback)
		{
			for (const BarrierRecord& barrier : barrierPlan_)
			{
				if (barrier.placement == BarrierPlacement::AfterGraph)
				{
					barrierCallback(barrier);
				}
			}
		}
		return true;
	}

	const RenderGraph::ResourceLifetime* RenderGraph::GetResourceLifetime(ResourceHandle handle) const
	{
		return ValidateResourceHandle(handle) ? &resources_[handle.id].lifetime : nullptr;
	}

	const std::vector<RenderGraph::ResourceAccess>* RenderGraph::GetPassAccesses(PassHandle handle) const
	{
		return handle.IsValid() && handle.id < passes_.size() ? &passes_[handle.id].accesses : nullptr;
	}

	std::string_view RenderGraph::GetResourceName(ResourceHandle handle) const
	{
		return ValidateResourceHandle(handle) ? std::string_view(resources_[handle.id].name) : std::string_view{};
	}

	std::string_view RenderGraph::GetPassName(PassHandle handle) const
	{
		return handle.IsValid() && handle.id < passes_.size() ? std::string_view(passes_[handle.id].name) : std::string_view{};
	}

	bool RenderGraph::ValidateResourceHandle(ResourceHandle handle) const
	{
		return handle.IsValid() && handle.id < resources_.size();
	}
} // namespace Ken4lowEngine
