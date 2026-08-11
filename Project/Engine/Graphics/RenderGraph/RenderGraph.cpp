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
		compileStats_ = {};
		compiled_ = false;
	}

	RenderGraph::ResourceHandle RenderGraph::CreateResource(std::string name, bool imported)
	{
		ResourceNode node{};
		node.name = std::move(name);
		node.lifetime.imported = imported;
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
		accessMasks.assign(resources_.size(), 0u);

		for (uint32_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			const PassNode& pass = passes_[passIndex];
			for (uint32_t dependency : pass.explicitDependencies)
			{
				addDependencyRecord(dependency, passIndex, {}, HazardType::Explicit);
			}

			std::fill(accessMasks.begin(), accessMasks.end(), 0u);
			for (const ResourceAccess& access : pass.accesses)
			{
				ResourceNode& resource = resources_[access.resource.id];
				resource.lifetime.firstPass = (std::min)(resource.lifetime.firstPass, static_cast<std::size_t>(passIndex));
				resource.lifetime.lastPass = (std::max)(resource.lifetime.lastPass, static_cast<std::size_t>(passIndex));
				uint8_t& mask = accessMasks[access.resource.id];
				if (access.access == AccessType::Read || access.access == AccessType::ReadWrite) mask |= 0x1u;
				if (access.access == AccessType::Write || access.access == AccessType::ReadWrite) mask |= 0x2u;
			}

			// Read/read access remains parallel; only RAW, WAR and WAW hazards generate ordering edges.
			for (uint32_t resourceIndex = 0; resourceIndex < accessMasks.size(); ++resourceIndex)
			{
				const uint8_t mask = accessMasks[resourceIndex];
				if (mask == 0u) continue;
				const bool reads = (mask & 0x1u) != 0u;
				const bool writes = (mask & 0x2u) != 0u;
				const ResourceHandle handle{ resourceIndex };

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

		compiled_ = true;
		return true;
	}

	bool RenderGraph::Execute(std::string* outError)
	{
		if (!compiled_ && !Compile(outError)) return false;
		for (uint32_t passIndex : compiledOrder_)
		{
			if (passIndex >= passes_.size()) continue;
			const ExecuteCallback& callback = passes_[passIndex].execute;
			if (callback) callback();
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
