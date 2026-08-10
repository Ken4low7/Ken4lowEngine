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
		std::vector<ResourceHandle> reads,
		std::vector<ResourceHandle> writes,
		ExecuteCallback execute)
	{
		PassNode node{};
		node.name = std::move(name);
		node.reads = std::move(reads);
		node.writes = std::move(writes);
		node.execute = std::move(execute);
		passes_.push_back(std::move(node));
		compiled_ = false;
		return { static_cast<uint32_t>(passes_.size() - 1) };
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
			for (ResourceHandle handle : pass.reads)
			{
				if (!ValidateResourceHandle(handle))
				{
					if (outError) *outError = "Render GraphのRead Resource Handleが無効です: " + pass.name;
					return false;
				}
			}
			for (ResourceHandle handle : pass.writes)
			{
				if (!ValidateResourceHandle(handle))
				{
					if (outError) *outError = "Render GraphのWrite Resource Handleが無効です: " + pass.name;
					return false;
				}
			}
		}

		std::pmr::memory_resource* scratch = FrameMemory::GetInstance()->GetMemoryResource();
		std::pmr::vector<uint32_t> indegree(scratch);
		indegree.assign(passes_.size(), 0u);
		std::pmr::vector<int32_t> lastAccess(scratch);
		lastAccess.assign(resources_.size(), -1);
		std::vector<std::vector<uint32_t>> adjacency(passes_.size());

		auto addEdge = [&adjacency, &indegree, this](uint32_t before, uint32_t after)
			{
				if (before == after || before >= passes_.size() || after >= passes_.size()) return;
				auto& edges = adjacency[before];
				if (std::find(edges.begin(), edges.end(), after) != edges.end()) return;
				edges.push_back(after);
				++indegree[after];
				++compileStats_.dependencyCount;
			};

		for (uint32_t passIndex = 0; passIndex < passes_.size(); ++passIndex)
		{
			const PassNode& pass = passes_[passIndex];
			for (uint32_t dependency : pass.explicitDependencies) addEdge(dependency, passIndex);

			auto registerAccess = [&](ResourceHandle handle)
				{
					ResourceNode& resource = resources_[handle.id];
					resource.lifetime.firstPass = (std::min)(resource.lifetime.firstPass, static_cast<std::size_t>(passIndex));
					resource.lifetime.lastPass = (std::max)(resource.lifetime.lastPass, static_cast<std::size_t>(passIndex));
					const int32_t previousPass = lastAccess[handle.id];
					if (previousPass >= 0 && static_cast<uint32_t>(previousPass) != passIndex)
					{
						addEdge(static_cast<uint32_t>(previousPass), passIndex);
					}
					lastAccess[handle.id] = static_cast<int32_t>(passIndex);
				};

			for (ResourceHandle handle : pass.reads) registerAccess(handle);
			for (ResourceHandle handle : pass.writes) registerAccess(handle);
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

	std::string_view RenderGraph::GetPassName(PassHandle handle) const
	{
		return handle.IsValid() && handle.id < passes_.size() ? std::string_view(passes_[handle.id].name) : std::string_view{};
	}

	bool RenderGraph::ValidateResourceHandle(ResourceHandle handle) const
	{
		return handle.IsValid() && handle.id < resources_.size();
	}
} // namespace Ken4lowEngine
