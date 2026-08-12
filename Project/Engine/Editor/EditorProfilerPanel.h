#pragma once

#include <GameTimer.h>
#include <PerformanceMonitor.h>
#include <Engine/Core/Concurrency/JobSystem.h>
#include <Engine/Graphics/Descriptor/SRV/SRVManager.h>
#include <Engine/Graphics/Device/Facade/DirectXCommon.h>
#include <Engine/Graphics/Pipeline/RenderPipelineController.h>
#include <Engine/Graphics/RenderGraph/RenderGraphVisualizer.h>
#include <Engine/Scene/Management/SceneManager.h>
#include <Engine/Scene/Streaming/WorldPartitionManager.h>

#include <cstddef>
#include <string_view>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	/// <summary>
	/// Frame / Render / Job-System / Descriptor-Cache / World診断を一つのEditor Profilerへ集約します。
	/// 各Subsystemが所有する既存Counterだけを読み取り、Profiler専用のschedule/lifetime mirrorは持ちません。
	/// </summary>
	class EditorProfilerPanel final
	{
	public:
		static EditorProfilerPanel* GetInstance()
		{
			static EditorProfilerPanel instance;
			return &instance;
		}

		void SetVisible(bool visible) { visible_ = visible; }
		[[nodiscard]] bool IsVisible() const { return visible_; }

		void Draw(SceneManager* sceneManager, const PerformanceMonitor* performanceMonitor)
		{
#ifdef USE_IMGUI
			if (ImGui::IsKeyPressed(ImGuiKey_F11, false))
			{
				visible_ = !visible_;
			}
			if (!visible_)
			{
				return;
			}

			if (!ImGui::Begin("Profiler###EditorUnifiedProfiler", &visible_, ImGuiWindowFlags_MenuBar))
			{
				ImGui::End();
				return;
			}

			if (ImGui::BeginMenuBar())
			{
				ImGui::TextDisabled("F11: toggle / read-only subsystem diagnostics");
				ImGui::EndMenuBar();
			}

			// Phase 11.5はSubsystem自身の診断値を直接読むため、Profiler側に二重Counterを作らない。
			DrawFrame(performanceMonitor);
			DrawRender();
			DrawJobsAndSystems(sceneManager);
			DrawDescriptorsAndCaches();
			DrawWorldPartition();

			ImGui::End();
#else
			(void)sceneManager;
			(void)performanceMonitor;
#endif // USE_IMGUI
		}

	private:
		EditorProfilerPanel() = default;
		~EditorProfilerPanel() = default;
		EditorProfilerPanel(const EditorProfilerPanel&) = delete;
		EditorProfilerPanel& operator=(const EditorProfilerPanel&) = delete;

#ifdef USE_IMGUI
		static void DrawFrame(const PerformanceMonitor* performanceMonitor)
		{
			ImGui::SeparatorText("Frame");
			const GameTimer* timer = GameTimer::GetInstance();
			const GameTimer::CompletedFrameTiming& timing = timer->GetCompletedFrameTiming();
			const float instantFps = timing.frameIntervalMs > 0.0f ? 1000.0f / timing.frameIntervalMs : 0.0f;

			if (ImGui::BeginTable("##UnifiedProfilerFrame", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("FPS", "%.1f", instantFps);
				DrawMetric("Frame Interval", "%.2f ms", timing.frameIntervalMs);
				DrawMetric("Total", "%.2f ms", timing.totalFrameMs);
				DrawMetric("Target", "%d FPS", timer->GetTargetFPS());
				DrawMetric("Update", "%.2f ms", timing.updateMs);
				DrawMetric("Draw", "%.2f ms", timing.drawMs);
				DrawMetric("Present", "%.2f ms", timing.presentMs);
				if (performanceMonitor)
				{
					const PerformanceStats& stats = performanceMonitor->GetStats();
					DrawMetric("CPU", "%.1f %%", stats.cpuUsagePercent);
					DrawMetric("Process CPU", "%.1f %%", stats.processCpuUsagePercent);
					DrawMetric("Memory", "%.1f MB", stats.memoryUsageMB);
					DrawMetric("Tracked Assets", "%.1f MB", stats.trackedAssetMemoryMB);
				}
				ImGui::EndTable();
			}
		}

		static void DrawRender()
		{
			ImGui::SeparatorText("Render");
			RenderPipelineController* controller = RenderPipelineController::GetActiveController();
			if (!controller)
			{
				ImGui::TextDisabled("Active RenderPipelineController is not available.");
				return;
			}

			const RenderGraph::CompileStats& graphStats = controller->GetRenderGraph().GetCompileStats();
			const RenderGraphTransientPool::Stats& transientStats = controller->GetRenderGraphTransientPool().GetStats();
			const RenderPipelineController::PerformanceMetric& gpuFrame = controller->GetGpuFrameMetric();

			if (ImGui::BeginTable("##UnifiedProfilerRender", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("Passes", "%zu / %zu", graphStats.executedPassCount, graphStats.passCount);
				DrawMetric("Culled", "%zu", graphStats.culledPassCount);
				DrawMetric("Resources", "%zu", graphStats.resourceCount);
				DrawMetric("Dependencies", "%zu", graphStats.dependencyCount);
				DrawMetric("RAW / WAR / WAW", "%zu / %zu / %zu", graphStats.rawHazardCount, graphStats.warHazardCount, graphStats.wawHazardCount);
				DrawMetric("Transition / UAV", "%zu / %zu", graphStats.transitionBarrierCount, graphStats.uavBarrierCount);
				DrawMetric("Unknown State", "%zu", graphStats.unknownStateAccessCount);
				DrawMetric("Transient Slots", "%zu", transientStats.physicalSlotCount);
				DrawMetric("Alias Reuse", "%zu", transientStats.aliasingReuseCount);
				DrawMetric("Transient Saved", "%.2f MB", BytesToMiB(transientStats.savedBytes));
				DrawMetric("GPU Frame Last", "%.2f ms", gpuFrame.lastMs);
				DrawMetric("GPU Frame Avg", "%.2f ms", gpuFrame.averageMs);
				ImGui::EndTable();
			}

			if (ImGui::Button("Open RenderGraph Visualizer"))
			{
				RenderGraphVisualizer::GetInstance()->SetVisible(true);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Detailed pass order, lifetime, hazards and barriers stay in the RenderGraph-owned view.");
		}

		static void DrawJobsAndSystems(SceneManager* sceneManager)
		{
			ImGui::SeparatorText("Jobs / Systems");
			const JobSystem* jobSystem = JobSystem::GetInstance();
			if (ImGui::BeginTable("##UnifiedProfilerJobs", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("JobSystem", "%s", jobSystem->IsInitialized() ? "Initialized" : "Stopped");
				DrawMetric("Workers", "%zu", jobSystem->GetWorkerCount());
				DrawMetric("Pending Jobs", "%zu", jobSystem->GetPendingJobCount());
				ImGui::EndTable();
			}

			const BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			const SystemScheduler* scheduler = scene ? scene->GetEditorSystemScheduler() : nullptr;
			if (!scheduler)
			{
				ImGui::TextDisabled("Current scene does not expose a SystemScheduler diagnostic view.");
				return;
			}

			const SystemScheduleStats& stats = scheduler->GetStats();
			if (ImGui::BeginTable("##UnifiedProfilerSystems", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("Systems", "%zu", stats.systemCount);
				DrawMetric("Dependencies", "%zu", stats.dependencyCount);
				DrawMetric("Main / Worker", "%zu / %zu", stats.mainThreadSystemCount, stats.workerSystemCount);
				DrawMetric("Explicit", "%zu", stats.explicitDependencyCount);
				DrawMetric("RAW", "%zu", stats.rawHazardCount);
				DrawMetric("WAR", "%zu", stats.warHazardCount);
				DrawMetric("WAW", "%zu", stats.wawHazardCount);
				DrawMetric("Compiled", "%s", scheduler->IsCompiled() ? "Yes" : "No");
				ImGui::EndTable();
			}

			if (ImGui::TreeNode("Compiled System Order"))
			{
				const std::vector<SystemHandle>& order = scheduler->GetCompiledOrder();
				for (std::size_t index = 0; index < order.size(); ++index)
				{
					const SystemHandle handle = order[index];
					const std::string_view name = scheduler->GetSystemName(handle);
					const char* policy = scheduler->GetExecutionPolicy(handle) == SystemExecutionPolicy::Worker ? "Worker" : "MainThread";
					ImGui::Text("%zu. %.*s [%s]", index, static_cast<int>(name.size()), name.data(), policy);
				}
				ImGui::TreePop();
			}
		}

		static void DrawDescriptorsAndCaches()
		{
			ImGui::SeparatorText("Descriptors / Caches");
			const SRVManager::DescriptorStats descriptorStats = SRVManager::GetInstance()->GetDescriptorStats();
			if (ImGui::BeginTable("##UnifiedProfilerDescriptors", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("Persistent", "%u / %u", descriptorStats.persistentInUse, descriptorStats.persistentCapacity);
				DrawMetric("Persistent High", "%u", descriptorStats.persistentHighWater);
				DrawMetric("Transient", "%u / %u", descriptorStats.transientInUse, descriptorStats.transientCapacity);
				DrawMetric("Transient / Frame", "%u", descriptorStats.transientCapacityPerFrame);
				DrawMetric("Transient High", "%u", descriptorStats.transientHighWater);
				DrawMetric("Transient Alloc", "%llu", static_cast<unsigned long long>(descriptorStats.transientAllocationCount));
				DrawMetric("Reclaimed", "%llu", static_cast<unsigned long long>(descriptorStats.transientReclaimedCount));
				DrawMetric("Exhaustion", "%llu", static_cast<unsigned long long>(descriptorStats.exhaustionCount));
				ImGui::EndTable();
			}

			RenderPipelineController* controller = RenderPipelineController::GetActiveController();
			DirectXCommon* dxCommon = controller ? controller->GetDirectXCommon() : nullptr;
			DXCCompilerManager* compiler = dxCommon ? dxCommon->GetDXCCompilerManager() : nullptr;
			if (!dxCommon || !compiler)
			{
				ImGui::TextDisabled("Shader / PSO cache diagnostics are not available before renderer initialization.");
				return;
			}

			const DXCCompilerManager::ShaderCacheStats shaderStats = compiler->GetShaderCacheStats();
			const PipelineFactory::PipelineCacheStats psoStats = dxCommon->GetPipelineFactory().GetCacheStats();
			if (ImGui::BeginTable("##UnifiedProfilerCaches", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("Shader Requests", "%llu", static_cast<unsigned long long>(shaderStats.requestCount));
				DrawMetric("Shader Hit / Miss", "%llu / %llu", static_cast<unsigned long long>(shaderStats.hitCount), static_cast<unsigned long long>(shaderStats.missCount));
				DrawMetric("Shader Compile", "%llu", static_cast<unsigned long long>(shaderStats.compileCount));
				DrawMetric("Shader Entries", "%u", shaderStats.entryCount);
				DrawMetric("PSO Requests", "%llu", static_cast<unsigned long long>(psoStats.requestCount));
				DrawMetric("PSO Hit / Miss", "%llu / %llu", static_cast<unsigned long long>(psoStats.hitCount), static_cast<unsigned long long>(psoStats.missCount));
				DrawMetric("PSO Create", "%llu", static_cast<unsigned long long>(psoStats.createCount));
				DrawMetric("PSO Entries", "%u", psoStats.entryCount);
				ImGui::EndTable();
			}
		}

		static void DrawWorldPartition()
		{
			ImGui::SeparatorText("World Streaming");
			const WorldPartitionManager* partition = WorldPartitionManager::GetInstance();
			if (!partition->IsConfigured())
			{
				ImGui::TextDisabled("WorldPartitionManager is not configured for the current world.");
				return;
			}

			const LevelWorldPartitionSettings& settings = partition->GetSettings();
			const WorldPartitionCell sourceCell = partition->GetStreamingSourceCell();
			if (ImGui::BeginTable("##UnifiedProfilerWorld", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("Enabled", "%s", partition->IsEnabled() ? "Yes" : "No");
				DrawMetric("Source Cell", "%d, %d", sourceCell.x, sourceCell.z);
				DrawMetric("Loaded SubLevels", "%zu / %zu", partition->GetLoadedSubLevelCount(), partition->GetSubLevels().size());
				DrawMetric("Cell Size", "%.1f", settings.cellSize);
				DrawMetric("Load Radius", "%d", settings.loadRadiusCells);
				DrawMetric("Unload Radius", "%d", settings.unloadRadiusCells);
				ImGui::EndTable();
			}
		}

		template <typename... Args>
		static void DrawMetric(const char* label, const char* format, Args... args)
		{
			ImGui::TableNextColumn();
			ImGui::TextDisabled("%s", label);
			ImGui::Text(format, args...);
		}

		static float BytesToMiB(std::size_t bytes)
		{
			return static_cast<float>(bytes) / (1024.0f * 1024.0f);
		}
#endif // USE_IMGUI

		bool visible_ = false;
	};
} // namespace Ken4lowEngine
