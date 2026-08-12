#pragma once

#include <Engine/Graphics/Descriptor/SRV/SRVManager.h>
#include <Engine/Graphics/Device/Facade/DirectXCommon.h>
#include <Engine/Graphics/Pipeline/RenderPipelineController.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	/// <summary>
	/// Compile済みRenderGraphと既存Renderer診断値だけを読み取るDebug Visualizer。
	/// Scheduling / Barrier / Lifetimeを再計算せず、RenderGraph自身を唯一の真実として表示する。
	/// </summary>
	class RenderGraphVisualizer
	{
	public:
		static RenderGraphVisualizer* GetInstance()
		{
			static RenderGraphVisualizer instance;
			return &instance;
		}

		void SetVisible(bool visible) { visible_ = visible; }
		[[nodiscard]] bool IsVisible() const { return visible_; }

		void Draw(RenderPipelineController& controller)
		{
#ifdef USE_IMGUI
			if (ImGui::IsKeyPressed(ImGuiKey_F10, false))
			{
				visible_ = !visible_;
			}
			if (!visible_)
			{
				return;
			}

			if (!ImGui::Begin("Render Graph Visualizer###RenderGraphVisualizer", &visible_, ImGuiWindowFlags_MenuBar))
			{
				ImGui::End();
				return;
			}

			DrawMenuBar();
			const RenderGraph& graph = controller.GetRenderGraph();
			const RenderGraph::CompileStats& stats = graph.GetCompileStats();
			ImGui::TextDisabled("F10: 表示切替 / compiled graph metadata is read-only");
			DrawSummary(stats);

			if (ImGui::BeginTabBar("##RenderGraphVisualizerTabs", ImGuiTabBarFlags_Reorderable))
			{
				if (ImGui::BeginTabItem("Passes"))
				{
					DrawPasses(graph);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Resources"))
				{
					DrawResources(graph);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Hazards"))
				{
					DrawHazards(graph);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Barriers"))
				{
					DrawBarriers(graph);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Transient"))
				{
					DrawTransient(graph, controller.GetRenderGraphTransientPool());
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Descriptors"))
				{
					DrawDescriptors();
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Caches"))
				{
					DrawCaches(controller);
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}

			ImGui::End();
#else
			(void)controller;
#endif // USE_IMGUI
		}

	private:
		RenderGraphVisualizer() = default;
		~RenderGraphVisualizer() = default;
		RenderGraphVisualizer(const RenderGraphVisualizer&) = delete;
		RenderGraphVisualizer& operator=(const RenderGraphVisualizer&) = delete;

#ifdef USE_IMGUI
		static const char* ToString(RenderGraph::AccessType access)
		{
			switch (access)
			{
			case RenderGraph::AccessType::Read: return "Read";
			case RenderGraph::AccessType::Write: return "Write";
			case RenderGraph::AccessType::ReadWrite: return "ReadWrite";
			default: return "?";
			}
		}

		static const char* ToString(RenderGraph::ResourceState state)
		{
			switch (state)
			{
			case RenderGraph::ResourceState::Unknown: return "Unknown";
			case RenderGraph::ResourceState::Common: return "Common";
			case RenderGraph::ResourceState::RenderTarget: return "RenderTarget";
			case RenderGraph::ResourceState::DepthWrite: return "DepthWrite";
			case RenderGraph::ResourceState::DepthRead: return "DepthRead";
			case RenderGraph::ResourceState::ShaderResource: return "ShaderResource";
			case RenderGraph::ResourceState::UnorderedAccess: return "UAV";
			case RenderGraph::ResourceState::CopySource: return "CopySource";
			case RenderGraph::ResourceState::CopyDestination: return "CopyDestination";
			case RenderGraph::ResourceState::Present: return "Present";
			default: return "?";
			}
		}

		static const char* ToString(RenderGraph::HazardType hazard)
		{
			switch (hazard)
			{
			case RenderGraph::HazardType::Explicit: return "Explicit";
			case RenderGraph::HazardType::ReadAfterWrite: return "RAW";
			case RenderGraph::HazardType::WriteAfterRead: return "WAR";
			case RenderGraph::HazardType::WriteAfterWrite: return "WAW";
			default: return "?";
			}
		}

		static const char* ToString(RenderGraph::BarrierType type)
		{
			return type == RenderGraph::BarrierType::Transition ? "Transition" : "UAV";
		}

		static const char* ToString(RenderGraph::BarrierPlacement placement)
		{
			return placement == RenderGraph::BarrierPlacement::BeforePass ? "BeforePass" : "AfterGraph";
		}

		void DrawMenuBar()
		{
			if (!ImGui::BeginMenuBar())
			{
				return;
			}
			if (ImGui::BeginMenu("View"))
			{
				ImGui::MenuItem("Show IDs", nullptr, &showIds_);
				ImGui::MenuItem("Show Explicit Dependencies", nullptr, &showExplicitDependencies_);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}

		static void DrawSummary(const RenderGraph::CompileStats& stats)
		{
			if (ImGui::BeginTable("##RenderGraphSummary", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Passes\n%zu / %zu", stats.executedPassCount, stats.passCount);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("Culled\n%zu", stats.culledPassCount);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("Resources\n%zu", stats.resourceCount);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("Dependencies\n%zu", stats.dependencyCount);
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("RAW / WAR / WAW\n%zu / %zu / %zu", stats.rawHazardCount, stats.warHazardCount, stats.wawHazardCount);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("Transitions / UAV\n%zu / %zu", stats.transitionBarrierCount, stats.uavBarrierCount);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("Unknown State\n%zu", stats.unknownStateAccessCount);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("Outputs / Side Effects\n%zu / %zu", stats.outputResourceCount, stats.sideEffectPassCount);
				ImGui::EndTable();
			}
		}

		void DrawPasses(const RenderGraph& graph) const
		{
			std::vector<std::size_t> scheduleByPass(graph.GetPassCount(), (std::numeric_limits<std::size_t>::max)());
			for (std::size_t scheduleIndex = 0; scheduleIndex < graph.GetCompiledPassCount(); ++scheduleIndex)
			{
				const RenderGraph::PassHandle pass = graph.GetCompiledPassHandle(scheduleIndex);
				if (pass.IsValid() && pass.id < scheduleByPass.size())
				{
					scheduleByPass[pass.id] = scheduleIndex;
				}
			}

			if (ImGui::BeginTable("##RenderGraphPasses", 5,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
				ImVec2(0.0f, 0.0f)))
			{
				ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 62.0f);
				ImGui::TableSetupColumn("Pass", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 82.0f);
				ImGui::TableSetupColumn("Root", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				ImGui::TableSetupColumn("Resource Access", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				for (std::size_t passIndex = 0; passIndex < graph.GetPassCount(); ++passIndex)
				{
					const RenderGraph::PassHandle pass{ static_cast<uint32_t>(passIndex) };
					const bool culled = graph.IsPassCulled(pass);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (scheduleByPass[passIndex] == (std::numeric_limits<std::size_t>::max)()) ImGui::TextUnformatted("-");
					else ImGui::Text("%zu", scheduleByPass[passIndex]);
					ImGui::TableSetColumnIndex(1);
					if (showIds_) ImGui::Text("[%u] %.*s", pass.id, static_cast<int>(graph.GetPassName(pass).size()), graph.GetPassName(pass).data());
					else ImGui::TextUnformatted(graph.GetPassName(pass).data(), graph.GetPassName(pass).data() + graph.GetPassName(pass).size());
					ImGui::TableSetColumnIndex(2);
					if (culled) ImGui::TextDisabled("Culled");
					else ImGui::TextUnformatted("Executed");
					ImGui::TableSetColumnIndex(3);
					ImGui::TextUnformatted(graph.IsPassSideEffect(pass) ? "SideEffect" : "-");
					ImGui::TableSetColumnIndex(4);
					const std::vector<RenderGraph::ResourceAccess>* accesses = graph.GetPassAccesses(pass);
					if (!accesses || accesses->empty())
					{
						ImGui::TextDisabled("none");
					}
					else
					{
						for (std::size_t accessIndex = 0; accessIndex < accesses->size(); ++accessIndex)
						{
							const RenderGraph::ResourceAccess& access = (*accesses)[accessIndex];
							if (accessIndex > 0) ImGui::SameLine(0.0f, 4.0f);
							const std::string_view resourceName = graph.GetResourceName(access.resource);
							ImGui::Text("%.*s(%s,%s)%s",
								static_cast<int>(resourceName.size()), resourceName.data(), ToString(access.access), ToString(access.state),
								accessIndex + 1 < accesses->size() ? "," : "");
						}
					}
				}
				ImGui::EndTable();
			}
		}

		void DrawResources(const RenderGraph& graph) const
		{
			if (ImGui::BeginTable("##RenderGraphResources", 6,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
				ImVec2(0.0f, 0.0f)))
			{
				ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Owner", ImGuiTableColumnFlags_WidthFixed, 85.0f);
				ImGui::TableSetupColumn("Output", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableSetupColumn("Lifetime", ImGuiTableColumnFlags_WidthFixed, 95.0f);
				ImGui::TableSetupColumn("Initial", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableSetupColumn("Final", ImGuiTableColumnFlags_WidthFixed, 105.0f);
				ImGui::TableHeadersRow();

				for (std::size_t resourceIndex = 0; resourceIndex < graph.GetResourceCount(); ++resourceIndex)
				{
					const RenderGraph::ResourceHandle resource{ static_cast<uint32_t>(resourceIndex) };
					const RenderGraph::ResourceLifetime* lifetime = graph.GetResourceLifetime(resource);
					const std::string_view name = graph.GetResourceName(resource);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (showIds_) ImGui::Text("[%u] %.*s", resource.id, static_cast<int>(name.size()), name.data());
					else ImGui::TextUnformatted(name.data(), name.data() + name.size());
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(lifetime && lifetime->imported ? "Imported" : "Transient");
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(graph.IsResourceOutput(resource) ? "Yes" : "-");
					ImGui::TableSetColumnIndex(3);
					if (!lifetime || lifetime->firstPass == (std::numeric_limits<std::size_t>::max)()) ImGui::TextDisabled("unused");
					else ImGui::Text("%zu..%zu", lifetime->firstPass, lifetime->lastPass);
					ImGui::TableSetColumnIndex(4);
					ImGui::TextUnformatted(ToString(graph.GetResourceInitialState(resource)));
					ImGui::TableSetColumnIndex(5);
					ImGui::TextUnformatted(ToString(graph.GetResourceFinalState(resource)));
				}
				ImGui::EndTable();
			}
		}

		void DrawHazards(const RenderGraph& graph) const
		{
			const RenderGraph::CompileStats& stats = graph.GetCompileStats();
			ImGui::Text("RAW %zu / WAR %zu / WAW %zu / Unique Edges %zu",
				stats.rawHazardCount, stats.warHazardCount, stats.wawHazardCount, stats.dependencyCount);
			if (ImGui::BeginTable("##RenderGraphHazards", 4,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
				ImVec2(0.0f, 0.0f)))
			{
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn("Before", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("After", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Resource", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();
				for (const RenderGraph::DependencyRecord& dependency : graph.GetDependencies())
				{
					if (!showExplicitDependencies_ && dependency.hazard == RenderGraph::HazardType::Explicit) continue;
					const std::string_view before = graph.GetPassName(dependency.before);
					const std::string_view after = graph.GetPassName(dependency.after);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(ToString(dependency.hazard));
					ImGui::TableSetColumnIndex(1);
					ImGui::TextUnformatted(before.data(), before.data() + before.size());
					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(after.data(), after.data() + after.size());
					ImGui::TableSetColumnIndex(3);
					if (dependency.resource.IsValid())
					{
						const std::string_view resource = graph.GetResourceName(dependency.resource);
						ImGui::TextUnformatted(resource.data(), resource.data() + resource.size());
					}
					else ImGui::TextDisabled("explicit ordering");
				}
				ImGui::EndTable();
			}
		}

		static void DrawBarriers(const RenderGraph& graph)
		{
			const auto& barriers = graph.GetBarrierPlan();
			if (barriers.empty())
			{
				ImGui::TextDisabled("No graph-owned barriers. Unknown-state / owner-managed resources remain on the manual transition path.");
				return;
			}
			if (ImGui::BeginTable("##RenderGraphBarriers", 6,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY,
				ImVec2(0.0f, 0.0f)))
			{
				ImGui::TableSetupColumn("Type");
				ImGui::TableSetupColumn("Placement");
				ImGui::TableSetupColumn("Resource");
				ImGui::TableSetupColumn("Pass");
				ImGui::TableSetupColumn("Before");
				ImGui::TableSetupColumn("After");
				ImGui::TableHeadersRow();
				for (const RenderGraph::BarrierRecord& barrier : barriers)
				{
					const std::string_view resourceName = graph.GetResourceName(barrier.resource);
					const std::string_view passName = graph.GetPassName(barrier.pass);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(ToString(barrier.type));
					ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(ToString(barrier.placement));
					ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(resourceName.data(), resourceName.data() + resourceName.size());
					ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(passName.data(), passName.data() + passName.size());
					ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(ToString(barrier.before));
					ImGui::TableSetColumnIndex(5); ImGui::TextUnformatted(ToString(barrier.after));
				}
				ImGui::EndTable();
			}
		}

		static void DrawTransient(const RenderGraph& graph, const RenderGraphTransientPool& pool)
		{
			const RenderGraphTransientPool::Stats& stats = pool.GetStats();
			ImGui::Text("Graph transient logical resources: %zu", graph.GetCompileStats().transientResourceCount);
			ImGui::Text("Pool active / physical slots / alias reuses: %zu / %zu / %zu",
				stats.activeResourceCount, stats.physicalSlotCount, stats.aliasingReuseCount);
			ImGui::Text("Logical / Physical / Peak: %zu / %zu / %zu bytes",
				stats.logicalBytes, stats.physicalBytes, stats.peakLiveBytes);
			ImGui::Text("Saved / Fragmentation: %zu / %zu bytes", stats.savedBytes, stats.fragmentationBytes);

			const auto& allocations = pool.GetAllocations();
			if (allocations.empty())
			{
				ImGui::TextDisabled("No active placed-resource allocations. Current frame resources are still owner-managed committed resources.");
			}
			else if (ImGui::BeginTable("##TransientAllocations", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn("Resource");
				ImGui::TableSetupColumn("Slot");
				ImGui::TableSetupColumn("Bytes");
				ImGui::TableSetupColumn("Alignment");
				ImGui::TableSetupColumn("Aliased");
				ImGui::TableHeadersRow();
				for (const RenderGraphTransientPool::AllocationRecord& allocation : allocations)
				{
					const std::string_view name = graph.GetResourceName(allocation.resource);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name.data(), name.data() + name.size());
					ImGui::TableSetColumnIndex(1); ImGui::Text("%u", allocation.slotIndex);
					ImGui::TableSetColumnIndex(2); ImGui::Text("%zu", allocation.sizeBytes);
					ImGui::TableSetColumnIndex(3); ImGui::Text("%zu", allocation.alignmentBytes);
					ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(allocation.aliased ? "Yes" : "No");
				}
				ImGui::EndTable();
			}

			const auto& aliasing = pool.GetAliasingPlan();
			if (!aliasing.empty())
			{
				ImGui::SeparatorText("Aliasing Ownership Changes");
				for (const RenderGraphTransientPool::AliasingRecord& record : aliasing)
				{
					const std::string_view before = graph.GetResourceName(record.beforeResource);
					const std::string_view after = graph.GetResourceName(record.afterResource);
					const std::string_view pass = graph.GetPassName(record.beforePass);
					ImGui::BulletText("slot %u: %.*s -> %.*s before %.*s",
						record.slotIndex,
						static_cast<int>(before.size()), before.data(),
						static_cast<int>(after.size()), after.data(),
						static_cast<int>(pass.size()), pass.data());
				}
			}
		}

		static void DrawDescriptors()
		{
			const SRVManager::DescriptorStats stats = SRVManager::GetInstance()->GetDescriptorStats();
			const float persistentPressure = stats.persistentCapacity > 0
				? static_cast<float>(stats.persistentInUse) / static_cast<float>(stats.persistentCapacity)
				: 0.0f;
			const float transientPressure = stats.transientCapacity > 0
				? static_cast<float>(stats.transientInUse) / static_cast<float>(stats.transientCapacity)
				: 0.0f;

			ImGui::Text("Persistent: %u / %u (high-water %u)", stats.persistentInUse, stats.persistentCapacity, stats.persistentHighWater);
			ImGui::ProgressBar((std::min)(1.0f, persistentPressure), ImVec2(-1.0f, 0.0f));
			ImGui::Text("Transient: %u / %u (per-frame %u, high-water %u)",
				stats.transientInUse, stats.transientCapacity, stats.transientCapacityPerFrame, stats.transientHighWater);
			ImGui::ProgressBar((std::min)(1.0f, transientPressure), ImVec2(-1.0f, 0.0f));
			ImGui::Text("Frame %u / Allocations %llu / Reclaimed %llu / Recycles %llu / Exhaustion %llu",
				stats.currentFrameIndex,
				static_cast<unsigned long long>(stats.transientAllocationCount),
				static_cast<unsigned long long>(stats.transientReclaimedCount),
				static_cast<unsigned long long>(stats.transientFrameRecycleCount),
				static_cast<unsigned long long>(stats.exhaustionCount));
		}

		static void DrawCaches(RenderPipelineController& controller)
		{
			DirectXCommon* dxCommon = controller.GetDirectXCommon();
			if (!dxCommon)
			{
				ImGui::TextDisabled("DirectXCommon is unavailable.");
				return;
			}

			DXCCompilerManager* dxc = dxCommon->GetDXCCompilerManager();
			if (dxc)
			{
				const DXCCompilerManager::ShaderCacheStats shader = dxc->GetShaderCacheStats();
				ImGui::SeparatorText("Shader Cache");
				ImGui::Text("Requests %llu / Hits %llu / Misses %llu / Compiles %llu / Entries %u",
					static_cast<unsigned long long>(shader.requestCount),
					static_cast<unsigned long long>(shader.hitCount),
					static_cast<unsigned long long>(shader.missCount),
					static_cast<unsigned long long>(shader.compileCount),
					shader.entryCount);
				ImGui::Text("Invalidations %llu / Clears %llu",
					static_cast<unsigned long long>(shader.invalidationCount),
					static_cast<unsigned long long>(shader.clearCount));
				if (ImGui::Button("Clear Shader Cache")) dxc->ClearShaderCache();
			}

			PipelineFactory& pipelineFactory = dxCommon->GetPipelineFactory();
			const PipelineFactory::PipelineCacheStats pso = pipelineFactory.GetCacheStats();
			ImGui::SeparatorText("Graphics PSO Cache");
			ImGui::Text("Requests %llu / Hits %llu / Misses %llu / Creates %llu / Entries %u / Clears %llu",
				static_cast<unsigned long long>(pso.requestCount),
				static_cast<unsigned long long>(pso.hitCount),
				static_cast<unsigned long long>(pso.missCount),
				static_cast<unsigned long long>(pso.createCount),
				pso.entryCount,
				static_cast<unsigned long long>(pso.clearCount));
			if (ImGui::Button("Clear Graphics PSO Cache")) pipelineFactory.ClearCache();
		}
#endif // USE_IMGUI

		bool visible_ = false;
		bool showIds_ = false;
		bool showExplicitDependencies_ = true;
	};
} // namespace Ken4lowEngine
