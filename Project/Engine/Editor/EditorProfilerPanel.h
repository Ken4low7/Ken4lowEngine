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

			if (!ImGui::Begin("プロファイラー###EditorUnifiedProfiler", &visible_, ImGuiWindowFlags_MenuBar))
			{
				ImGui::End();
				return;
			}

			if (ImGui::BeginMenuBar())
			{
				ImGui::TextDisabled("F11: 表示切替 / 各サブシステムの読み取り専用診断");
				ImGui::EndMenuBar();
			}

			// 各サブシステムが持つ診断値を直接表示し、プロファイラー側で二重に計測しない。
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
			ImGui::SeparatorText("フレーム");
			const GameTimer* timer = GameTimer::GetInstance();
			const GameTimer::CompletedFrameTiming& timing = timer->GetCompletedFrameTiming();
			const float instantFps = timing.frameIntervalMs > 0.0f ? 1000.0f / timing.frameIntervalMs : 0.0f;

			if (ImGui::BeginTable("##UnifiedProfilerFrame", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("FPS", "%.1f", instantFps);
				DrawMetric("フレーム間隔", "%.2f ms", timing.frameIntervalMs);
				DrawMetric("合計時間", "%.2f ms", timing.totalFrameMs);
				DrawMetric("目標", "%d FPS", timer->GetTargetFPS());
				DrawMetric("更新処理", "%.2f ms", timing.updateMs);
				DrawMetric("描画処理", "%.2f ms", timing.drawMs);
				DrawMetric("画面反映", "%.2f ms", timing.presentMs);
				if (performanceMonitor)
				{
					const PerformanceStats& stats = performanceMonitor->GetStats();
					DrawMetric("CPU使用率", "%.1f %%", stats.cpuUsagePercent);
					DrawMetric("プロセスCPU", "%.1f %%", stats.processCpuUsagePercent);
					DrawMetric("メモリ使用量", "%.1f MB", stats.memoryUsageMB);
					DrawMetric("管理中アセット", "%.1f MB", stats.trackedAssetMemoryMB);
				}
				ImGui::EndTable();
			}
		}

		static void DrawRender()
		{
			ImGui::SeparatorText("描画");
			RenderPipelineController* controller = RenderPipelineController::GetActiveController();
			if (!controller)
			{
				ImGui::TextDisabled("使用中のRenderPipelineControllerを取得できません。");
				return;
			}

			const RenderGraph::CompileStats& graphStats = controller->GetRenderGraph().GetCompileStats();
			const RenderGraphTransientPool::Stats& transientStats = controller->GetRenderGraphTransientPool().GetStats();
			const RenderPipelineController::PerformanceMetric& gpuFrame = controller->GetGpuFrameMetric();

			if (ImGui::BeginTable("##UnifiedProfilerRender", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("実行パス / 全パス", "%zu / %zu", graphStats.executedPassCount, graphStats.passCount);
				DrawMetric("省略パス", "%zu", graphStats.culledPassCount);
				DrawMetric("リソース数", "%zu", graphStats.resourceCount);
				DrawMetric("依存関係", "%zu", graphStats.dependencyCount);
				DrawMetric("RAW / WAR / WAW", "%zu / %zu / %zu", graphStats.rawHazardCount, graphStats.warHazardCount, graphStats.wawHazardCount);
				DrawMetric("遷移 / UAVバリア", "%zu / %zu", graphStats.transitionBarrierCount, graphStats.uavBarrierCount);
				DrawMetric("不明な状態", "%zu", graphStats.unknownStateAccessCount);
				DrawMetric("一時スロット", "%zu", transientStats.physicalSlotCount);
				DrawMetric("エイリアス再利用", "%zu", transientStats.aliasingReuseCount);
				DrawMetric("一時メモリ削減量", "%.2f MB", BytesToMiB(transientStats.savedBytes));
				DrawMetric("GPUフレーム時間", "%.2f ms", gpuFrame.lastMs);
				DrawMetric("GPU平均フレーム時間", "%.2f ms", gpuFrame.averageMs);
				ImGui::EndTable();
			}

			if (ImGui::Button("RenderGraph可視化を開く"))
			{
				RenderGraphVisualizer::GetInstance()->SetVisible(true);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("詳細なパス順序・寿命・ハザード・バリアはRenderGraph可視化画面で確認できます。");
		}

		static void DrawJobsAndSystems(SceneManager* sceneManager)
		{
			ImGui::SeparatorText("ジョブ / システム");
			const JobSystem* jobSystem = JobSystem::GetInstance();
			if (ImGui::BeginTable("##UnifiedProfilerJobs", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("ジョブシステム", "%s", jobSystem->IsInitialized() ? "初期化済み" : "停止中");
				DrawMetric("ワーカースレッド", "%zu", jobSystem->GetWorkerCount());
				DrawMetric("待機中ジョブ", "%zu", jobSystem->GetPendingJobCount());
				ImGui::EndTable();
			}

			const BaseScene* scene = sceneManager ? sceneManager->GetCurrentScene() : nullptr;
			const SystemScheduler* scheduler = scene ? scene->GetEditorSystemScheduler() : nullptr;
			if (!scheduler)
			{
				ImGui::TextDisabled("現在のシーンではSystemSchedulerの診断情報を取得できません。");
				return;
			}

			const SystemScheduleStats& stats = scheduler->GetStats();
			if (ImGui::BeginTable("##UnifiedProfilerSystems", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("システム数", "%zu", stats.systemCount);
				DrawMetric("依存関係", "%zu", stats.dependencyCount);
				DrawMetric("メイン / ワーカー", "%zu / %zu", stats.mainThreadSystemCount, stats.workerSystemCount);
				DrawMetric("明示依存", "%zu", stats.explicitDependencyCount);
				DrawMetric("RAW", "%zu", stats.rawHazardCount);
				DrawMetric("WAR", "%zu", stats.warHazardCount);
				DrawMetric("WAW", "%zu", stats.wawHazardCount);
				DrawMetric("コンパイル済み", "%s", scheduler->IsCompiled() ? "はい" : "いいえ");
				ImGui::EndTable();
			}

			if (ImGui::TreeNode("システム実行順序"))
			{
				const std::vector<SystemHandle>& order = scheduler->GetCompiledOrder();
				for (std::size_t index = 0; index < order.size(); ++index)
				{
					const SystemHandle handle = order[index];
					const std::string_view name = scheduler->GetSystemName(handle);
					const char* policy = scheduler->GetExecutionPolicy(handle) == SystemExecutionPolicy::Worker ? "ワーカー" : "メインスレッド";
					ImGui::Text("%zu. %.*s [%s]", index, static_cast<int>(name.size()), name.data(), policy);
				}
				ImGui::TreePop();
			}
		}

		static void DrawDescriptorsAndCaches()
		{
			ImGui::SeparatorText("ディスクリプタ / キャッシュ");
			const SRVManager::DescriptorStats descriptorStats = SRVManager::GetInstance()->GetDescriptorStats();
			if (ImGui::BeginTable("##UnifiedProfilerDescriptors", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("永続使用量", "%u / %u", descriptorStats.persistentInUse, descriptorStats.persistentCapacity);
				DrawMetric("永続最大使用量", "%u", descriptorStats.persistentHighWater);
				DrawMetric("一時使用量", "%u / %u", descriptorStats.transientInUse, descriptorStats.transientCapacity);
				DrawMetric("1フレーム一時容量", "%u", descriptorStats.transientCapacityPerFrame);
				DrawMetric("一時最大使用量", "%u", descriptorStats.transientHighWater);
				DrawMetric("一時確保回数", "%llu", static_cast<unsigned long long>(descriptorStats.transientAllocationCount));
				DrawMetric("再利用回収数", "%llu", static_cast<unsigned long long>(descriptorStats.transientReclaimedCount));
				DrawMetric("枯渇回数", "%llu", static_cast<unsigned long long>(descriptorStats.exhaustionCount));
				ImGui::EndTable();
			}

			RenderPipelineController* controller = RenderPipelineController::GetActiveController();
			DirectXCommon* dxCommon = controller ? controller->GetDirectXCommon() : nullptr;
			DXCCompilerManager* compiler = dxCommon ? dxCommon->GetDXCCompilerManager() : nullptr;
			if (!dxCommon || !compiler)
			{
				ImGui::TextDisabled("レンダラー初期化前のためShader / PSOキャッシュ診断を利用できません。");
				return;
			}

			const DXCCompilerManager::ShaderCacheStats shaderStats = compiler->GetShaderCacheStats();
			const PipelineFactory::PipelineCacheStats psoStats = dxCommon->GetPipelineFactory().GetCacheStats();
			if (ImGui::BeginTable("##UnifiedProfilerCaches", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("Shader要求", "%llu", static_cast<unsigned long long>(shaderStats.requestCount));
				DrawMetric("Shaderヒット / ミス", "%llu / %llu", static_cast<unsigned long long>(shaderStats.hitCount), static_cast<unsigned long long>(shaderStats.missCount));
				DrawMetric("Shaderコンパイル", "%llu", static_cast<unsigned long long>(shaderStats.compileCount));
				DrawMetric("Shader登録数", "%u", shaderStats.entryCount);
				DrawMetric("PSO要求", "%llu", static_cast<unsigned long long>(psoStats.requestCount));
				DrawMetric("PSOヒット / ミス", "%llu / %llu", static_cast<unsigned long long>(psoStats.hitCount), static_cast<unsigned long long>(psoStats.missCount));
				DrawMetric("PSO生成", "%llu", static_cast<unsigned long long>(psoStats.createCount));
				DrawMetric("PSO登録数", "%u", psoStats.entryCount);
				ImGui::EndTable();
			}
		}

		static void DrawWorldPartition()
		{
			ImGui::SeparatorText("ワールドストリーミング");
			const WorldPartitionManager* partition = WorldPartitionManager::GetInstance();
			if (!partition->IsConfigured())
			{
				ImGui::TextDisabled("現在のワールドではWorldPartitionManagerが設定されていません。");
				return;
			}

			const LevelWorldPartitionSettings& settings = partition->GetSettings();
			const WorldPartitionCell sourceCell = partition->GetStreamingSourceCell();
			if (ImGui::BeginTable("##UnifiedProfilerWorld", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				DrawMetric("有効", "%s", partition->IsEnabled() ? "はい" : "いいえ");
				DrawMetric("基準セル", "%d, %d", sourceCell.x, sourceCell.z);
				DrawMetric("読込済みサブレベル", "%zu / %zu", partition->GetLoadedSubLevelCount(), partition->GetSubLevels().size());
				DrawMetric("セルサイズ", "%.1f", settings.cellSize);
				DrawMetric("読込半径", "%d", settings.loadRadiusCells);
				DrawMetric("解放半径", "%d", settings.unloadRadiusCells);
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
