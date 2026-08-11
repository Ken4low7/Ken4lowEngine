#include "RenderPipelineController.h"

#include "DirectXCommon.h"
#include "GameTimer.h"
#include "LightManager.h"
#include "ResourceManager.h"

#include <algorithm>
#include <cassert>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	RenderPipelineController* RenderPipelineController::activeController_ = nullptr;

	void RenderPipelineController::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		activeController_ = this;
		InitializeGpuTiming();
	}

	void RenderPipelineController::InitializeGpuTiming()
	{
		gpuTimingAvailable_ = false;
		gpuTimestampHeap_.Reset();
		gpuTimestampReadback_.Reset();
		gpuFrameStates_.clear();
		gpuTimestampFrequency_ = 0;
		gpuFrameResourceCount_ = 0;

		if (!dxCommon_ || !dxCommon_->GetDevice() || !dxCommon_->GetCommandManager())
		{
			return;
		}

		gpuFrameResourceCount_ = (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount());
		const uint32_t totalQueryCount = gpuFrameResourceCount_ * kGpuQueriesPerFrame;
		D3D12_QUERY_HEAP_DESC queryHeapDesc{};
		queryHeapDesc.Count = totalQueryCount;
		queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		queryHeapDesc.NodeMask = 0;
		if (FAILED(dxCommon_->GetDevice()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&gpuTimestampHeap_))))
		{
			return;
		}

		const UINT64 readbackBytes = static_cast<UINT64>(totalQueryCount) * sizeof(uint64_t);
		gpuTimestampReadback_ = ResourceManager::CreateBufferResource(
			dxCommon_->GetDevice(),
			readbackBytes,
			D3D12_HEAP_TYPE_READBACK,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_COPY_DEST);
		if (!gpuTimestampReadback_)
		{
			gpuTimestampHeap_.Reset();
			return;
		}

		if (FAILED(dxCommon_->GetCommandManager()->GetCommandQueue()->GetTimestampFrequency(&gpuTimestampFrequency_)) || gpuTimestampFrequency_ == 0)
		{
			gpuTimestampReadback_.Reset();
			gpuTimestampHeap_.Reset();
			return;
		}

		gpuTimestampHeap_->SetName(L"RenderPipeline GPU Timestamp Heap");
		gpuTimestampReadback_->SetName(L"RenderPipeline GPU Timestamp Readback");
		gpuFrameStates_.resize(gpuFrameResourceCount_);
		gpuTimingAvailable_ = true; // FrameResource再利用時だけReadbackすることでGPUを追加待機せず計測する。
	}

	uint32_t RenderPipelineController::GetGpuFrameBaseQuery(uint32_t frameIndex) const
	{
		return (frameIndex % (std::max)(1u, gpuFrameResourceCount_)) * kGpuQueriesPerFrame;
	}

	uint32_t RenderPipelineController::GetGpuPhaseQuery(uint32_t frameIndex, PerformancePhase phase, bool begin) const
	{
		return GetGpuFrameBaseQuery(frameIndex) + static_cast<uint32_t>(ToIndex(phase)) * kGpuQueriesPerPhase + (begin ? 0u : 1u);
	}

	uint32_t RenderPipelineController::GetGpuFrameTotalQuery(uint32_t frameIndex, bool begin) const
	{
		return GetGpuFrameBaseQuery(frameIndex) + static_cast<uint32_t>(kPerformancePhaseCount) * kGpuQueriesPerPhase + (begin ? 0u : 1u);
	}

	void RenderPipelineController::CollectGpuTiming(uint32_t frameIndex)
	{
		if (!gpuTimingAvailable_ || gpuFrameStates_.empty() || !gpuTimestampReadback_ || gpuTimestampFrequency_ == 0)
		{
			return;
		}

		const uint32_t safeFrameIndex = frameIndex % gpuFrameResourceCount_;
		GpuFrameState& state = gpuFrameStates_[safeFrameIndex];
		if (!state.pendingResolve)
		{
			return;
		}

		const uint32_t baseQuery = GetGpuFrameBaseQuery(safeFrameIndex);
		const SIZE_T beginByte = static_cast<SIZE_T>(baseQuery) * sizeof(uint64_t);
		const SIZE_T endByte = static_cast<SIZE_T>(baseQuery + kGpuQueriesPerFrame) * sizeof(uint64_t);
		const D3D12_RANGE readRange{ beginByte, endByte };
		void* mappedData = nullptr;
		if (FAILED(gpuTimestampReadback_->Map(0, &readRange, &mappedData)) || !mappedData)
		{
			return;
		}

		const auto* timestamps = static_cast<const uint64_t*>(mappedData);
		const double millisecondsPerTick = 1000.0 / static_cast<double>(gpuTimestampFrequency_);
		for (std::size_t phaseIndex = 0; phaseIndex < kPerformancePhaseCount; ++phaseIndex)
		{
			if (!state.recordedPhases[phaseIndex])
			{
				continue;
			}

			const PerformancePhase phase = static_cast<PerformancePhase>(phaseIndex);
			const uint32_t beginQuery = GetGpuPhaseQuery(safeFrameIndex, phase, true);
			const uint32_t endQuery = GetGpuPhaseQuery(safeFrameIndex, phase, false);
			if (timestamps[endQuery] >= timestamps[beginQuery])
			{
				const float elapsedMs = static_cast<float>(static_cast<double>(timestamps[endQuery] - timestamps[beginQuery]) * millisecondsPerTick);
				UpdateGpuPerformanceMetric(phase, elapsedMs);
			}
		}

		const uint32_t frameBeginQuery = GetGpuFrameTotalQuery(safeFrameIndex, true);
		const uint32_t frameEndQuery = GetGpuFrameTotalQuery(safeFrameIndex, false);
		if (timestamps[frameEndQuery] >= timestamps[frameBeginQuery])
		{
			const float frameMs = static_cast<float>(static_cast<double>(timestamps[frameEndQuery] - timestamps[frameBeginQuery]) * millisecondsPerTick);
			gpuFrameMetric_.lastMs = (std::max)(0.0f, frameMs);
			gpuFrameMetric_.averageMs = gpuFrameMetric_.sampleCount == 0 ? gpuFrameMetric_.lastMs : gpuFrameMetric_.averageMs * 0.9f + gpuFrameMetric_.lastMs * 0.1f;
			gpuFrameMetric_.maxMs = (std::max)(gpuFrameMetric_.maxMs, gpuFrameMetric_.lastMs);
			++gpuFrameMetric_.sampleCount;
		}

		const D3D12_RANGE writeRange{ 0, 0 };
		gpuTimestampReadback_->Unmap(0, &writeRange);
		state.pendingResolve = false;
		state.recordedPhases.fill(false);
	}

	void RenderPipelineController::BeginGpuFrame(uint32_t frameIndex)
	{
		if (!gpuTimingAvailable_ || gpuFrameStates_.empty())
		{
			return;
		}

		currentGpuFrameIndex_ = frameIndex % gpuFrameResourceCount_;
		GpuFrameState& state = gpuFrameStates_[currentGpuFrameIndex_];
		state.recordedPhases.fill(false);
		state.pendingResolve = false;
		dxCommon_->GetCommandManager()->GetCommandList()->EndQuery(
			gpuTimestampHeap_.Get(),
			D3D12_QUERY_TYPE_TIMESTAMP,
			GetGpuFrameTotalQuery(currentGpuFrameIndex_, true));
	}

	void RenderPipelineController::WriteGpuPhaseTimestamp(PerformancePhase phase, bool begin)
	{
		if (!gpuTimingAvailable_ || gpuFrameStates_.empty())
		{
			return;
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->EndQuery(
			gpuTimestampHeap_.Get(),
			D3D12_QUERY_TYPE_TIMESTAMP,
			GetGpuPhaseQuery(currentGpuFrameIndex_, phase, begin));
		gpuFrameStates_[currentGpuFrameIndex_].recordedPhases[ToIndex(phase)] = true;
	}

	void RenderPipelineController::EndGpuFrame(uint32_t frameIndex)
	{
		if (!gpuTimingAvailable_ || gpuFrameStates_.empty())
		{
			return;
		}

		const uint32_t safeFrameIndex = frameIndex % gpuFrameResourceCount_;
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const uint32_t frameEndQuery = GetGpuFrameTotalQuery(safeFrameIndex, false);
		commandList->EndQuery(gpuTimestampHeap_.Get(), D3D12_QUERY_TYPE_TIMESTAMP, frameEndQuery);

		GpuFrameState& state = gpuFrameStates_[safeFrameIndex];
		for (std::size_t phaseIndex = 0; phaseIndex < kPerformancePhaseCount; ++phaseIndex)
		{
			if (!state.recordedPhases[phaseIndex])
			{
				continue;
			}

			const PerformancePhase phase = static_cast<PerformancePhase>(phaseIndex);
			const uint32_t beginQuery = GetGpuPhaseQuery(safeFrameIndex, phase, true);
			const UINT64 destinationOffset = static_cast<UINT64>(beginQuery) * sizeof(uint64_t);
			commandList->ResolveQueryData(
				gpuTimestampHeap_.Get(),
				D3D12_QUERY_TYPE_TIMESTAMP,
				beginQuery,
				2,
				gpuTimestampReadback_.Get(),
				destinationOffset);
		}

		const uint32_t frameBeginQuery = GetGpuFrameTotalQuery(safeFrameIndex, true);
		commandList->ResolveQueryData(
			gpuTimestampHeap_.Get(),
			D3D12_QUERY_TYPE_TIMESTAMP,
			frameBeginQuery,
			2,
			gpuTimestampReadback_.Get(),
			static_cast<UINT64>(frameBeginQuery) * sizeof(uint64_t));
		state.pendingResolve = true;
	}

	void RenderPipelineController::ExecuteFrame(bool editorModeEnabled, const FrameCallbacks& callbacks)
	{
		if (!dxCommon_) return;

		const uint32_t frameIndex = dxCommon_->GetCommandManager()->GetCurrentFrameIndex();
		CollectGpuTiming(frameIndex);
		BeginGpuFrame(frameIndex);

		renderGraph_.Reset();
		const RenderGraph::ResourceHandle backBuffer = renderGraph_.CreateResource("BackBuffer", true);
		const RenderGraph::ResourceHandle shadowMap = renderGraph_.CreateResource("ShadowMap", true);
		const RenderGraph::ResourceHandle sceneColor = renderGraph_.CreateResource("SceneColor", true);
		const RenderGraph::ResourceHandle postColor = renderGraph_.CreateResource("PostColor", false);
		renderGraph_.MarkResourceOutput(backBuffer); // 最終表示先BackBufferをCulling sinkとして明示し、未使用Passだけを除外可能にする。
		RenderGraph::PassHandle previousPass{};

		auto addChainedPass = [this, &previousPass](
			std::string name,
			std::initializer_list<RenderGraph::ResourceHandle> reads,
			std::initializer_list<RenderGraph::ResourceHandle> writes,
			std::function<void()> execute)
			{
				RenderGraph::PassHandle pass = renderGraph_.AddPass(std::move(name), reads, writes, std::move(execute));
				if (previousPass.IsValid()) renderGraph_.AddDependency(previousPass, pass);
				previousPass = pass;
				return pass;
			};

		const RenderGraph::PassHandle beginDrawPass = addChainedPass("BeginDraw", {}, { backBuffer }, [this]()
			{
				MeasurePhase(PerformancePhase::BeginDraw, [this]() { dxCommon_->BeginDraw(); });
			});
		renderGraph_.MarkPassSideEffect(beginDrawPass);
		addChainedPass("ShadowPrepare", {}, { shadowMap }, [this, &callbacks]()
			{
				MeasurePhase(PerformancePhase::ShadowPrepare, callbacks.prepareShadowPass);
			});
		addChainedPass("ShadowRender", { shadowMap }, { shadowMap }, [this, &callbacks]()
			{
				MeasurePhase(PerformancePhase::ShadowRender, [&callbacks]()
					{
						LightManager::GetInstance()->ExecuteShadowPasses(callbacks.drawShadowObjects);
					});
			});

		if (editorModeEnabled)
		{
			const RenderGraph::PassHandle editorUiBuildPass = addChainedPass("EditorUiBuild", {}, {}, [this, &callbacks]() { MeasurePhase(PerformancePhase::EditorUiBuild, callbacks.buildEditorUi); });
			const RenderGraph::PassHandle editorPickingPass = addChainedPass("EditorPicking", {}, {}, [this, &callbacks]() { MeasurePhase(PerformancePhase::EditorPicking, callbacks.executeEditorPickingPass); });
			renderGraph_.MarkPassSideEffect(editorUiBuildPass);
			renderGraph_.MarkPassSideEffect(editorPickingPass); // Resourceを持たないEditor処理も外部状態へ作用するためCulling rootとして保持する。
			addChainedPass("MainWorldRender", { shadowMap }, { sceneColor }, [this, &callbacks]() { MeasurePhase(PerformancePhase::MainWorldRender, callbacks.drawGameWorldToSceneTarget); });
			addChainedPass("PostEffect", { sceneColor }, { postColor }, [this, &callbacks]() { MeasurePhase(PerformancePhase::PostEffect, callbacks.renderPostEffectToGameRenderTarget); });
			addChainedPass("SelectionOutline", { postColor }, { postColor }, [this, &callbacks]() { MeasurePhase(PerformancePhase::SelectionOutline, callbacks.renderEditorSelectionOutline); });
			addChainedPass("SceneOverlay", { postColor }, { postColor }, [this, &callbacks]()
				{
					MeasurePhase(PerformancePhase::SceneOverlay, [&callbacks]()
						{
							if (callbacks.beginGameRenderTargetOverlay) callbacks.beginGameRenderTargetOverlay();
							if (callbacks.drawScene2DOverlay) callbacks.drawScene2DOverlay();
							if (callbacks.endGameRenderTargetOverlay) callbacks.endGameRenderTargetOverlay();
						});
				});
			addChainedPass("ImGuiRender", { postColor }, { backBuffer }, [this, &callbacks]() { MeasurePhase(PerformancePhase::ImGuiRender, callbacks.drawImGuiOverlay); });
		}
		else
		{
			addChainedPass("MainWorldRender", { shadowMap }, { sceneColor }, [this, &callbacks]() { MeasurePhase(PerformancePhase::MainWorldRender, callbacks.drawGameWorldToSceneTarget); });
			addChainedPass("BackBufferPostEffect", { sceneColor }, { backBuffer }, [this, &callbacks]() { MeasurePhase(PerformancePhase::BackBufferPostEffect, callbacks.applyPostEffectToBackBuffer); });
			addChainedPass("BackBufferRebind", { backBuffer }, { backBuffer }, [this, &callbacks]() { MeasurePhase(PerformancePhase::BackBufferRebind, callbacks.rebindBackBufferForGameOverlay); });
			addChainedPass("GameUi", { backBuffer }, { backBuffer }, [this, &callbacks]() { MeasurePhase(PerformancePhase::GameUi, callbacks.drawGameUIToBackBuffer); });
		}

		std::string graphError;
		if (!renderGraph_.Compile(&graphError) || !renderGraph_.Execute(&graphError))
		{
			// Graph検証失敗時だけ旧固定順へFallbackし、描画そのものを失わない。
			MeasurePhase(PerformancePhase::BeginDraw, [this]() { dxCommon_->BeginDraw(); });
			ExecuteShadowMapPass(callbacks);
			if (editorModeEnabled) ExecuteEditorFrame(callbacks);
			else ExecuteGameFrame(callbacks);
		}

		EndGpuFrame(frameIndex);
	}

	void RenderPipelineController::MeasurePhase(PerformancePhase phase, const std::function<void()>& callback)
	{
		if (!callback) return;
		WriteGpuPhaseTimestamp(phase, true);
		const auto begin = Clock::now();
		callback();
		const float elapsedMs = std::chrono::duration<float, std::milli>(Clock::now() - begin).count();
		WriteGpuPhaseTimestamp(phase, false);
		UpdatePerformanceMetric(phase, elapsedMs);
	}

	void RenderPipelineController::UpdatePerformanceMetric(PerformancePhase phase, float elapsedMs)
	{
		PerformanceMetric& metric = performanceMetrics_[ToIndex(phase)];
		metric.lastMs = (std::max)(0.0f, elapsedMs);
		metric.averageMs = metric.sampleCount == 0 ? metric.lastMs : metric.averageMs * 0.9f + metric.lastMs * 0.1f;
		metric.maxMs = (std::max)(metric.maxMs, metric.lastMs);
		++metric.sampleCount;
	}

	void RenderPipelineController::UpdateGpuPerformanceMetric(PerformancePhase phase, float elapsedMs)
	{
		PerformanceMetric& metric = gpuPerformanceMetrics_[ToIndex(phase)];
		metric.lastMs = (std::max)(0.0f, elapsedMs);
		metric.averageMs = metric.sampleCount == 0 ? metric.lastMs : metric.averageMs * 0.9f + metric.lastMs * 0.1f;
		metric.maxMs = (std::max)(metric.maxMs, metric.lastMs);
		++metric.sampleCount;
	}

	const RenderPipelineController::PerformanceMetric& RenderPipelineController::GetPerformanceMetric(PerformancePhase phase) const
	{
		return performanceMetrics_[ToIndex(phase)];
	}

	const char* RenderPipelineController::GetPerformancePhaseName(PerformancePhase phase)
	{
		switch (phase)
		{
		case PerformancePhase::BeginDraw: return "BeginDraw";
		case PerformancePhase::ShadowPrepare: return "Shadow Prepare";
		case PerformancePhase::ShadowRender: return "Shadow Render";
		case PerformancePhase::EditorUiBuild: return "Editor UI Build";
		case PerformancePhase::EditorPicking: return "Editor Picking";
		case PerformancePhase::MainWorldRender: return "Main World Render";
		case PerformancePhase::PostEffect: return "PostEffect";
		case PerformancePhase::SelectionOutline: return "Selection Outline";
		case PerformancePhase::SceneOverlay: return "Scene Overlay";
		case PerformancePhase::ImGuiRender: return "ImGui Render";
		case PerformancePhase::BackBufferPostEffect: return "BackBuffer PostEffect";
		case PerformancePhase::BackBufferRebind: return "BackBuffer Rebind";
		case PerformancePhase::GameUi: return "Game UI";
		default: return "Unknown";
		}
	}

	void RenderPipelineController::DrawPerformanceImGui()
	{
#ifdef USE_IMGUI
		const GameTimer::CompletedFrameTiming& completed = GameTimer::GetInstance()->GetCompletedFrameTiming();
		frameTimingSummary_.frameIntervalMs = completed.frameIntervalMs;
		frameTimingSummary_.updateMs = completed.updateMs;
		frameTimingSummary_.drawMs = completed.drawMs;
		frameTimingSummary_.presentMs = completed.presentMs;
		frameTimingSummary_.totalFrameMs = completed.totalFrameMs;

		if (!ImGui::Begin("Render Pipeline Performance"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::Button("最大値リセット"))
		{
			for (PerformanceMetric& metric : performanceMetrics_) metric.maxMs = metric.lastMs;
			for (PerformanceMetric& metric : gpuPerformanceMetrics_) metric.maxMs = metric.lastMs;
			gpuFrameMetric_.maxMs = gpuFrameMetric_.lastMs;
		}

		if (dxCommon_)
		{
			bool framesInFlightEnabled = dxCommon_->IsFramesInFlightEnabled();
			if (ImGui::Checkbox("Experimental Frames in Flight", &framesInFlightEnabled))
			{
				dxCommon_->SetFramesInFlightEnabled(framesInFlightEnabled);
			}
			ImGui::SameLine();
			ImGui::TextDisabled("Active: %s", dxCommon_->IsFramesInFlightActive() ? "ON" : "OFF");
			ImGui::Text("Frame Resources: %u", dxCommon_->GetCommandManager()->GetFrameResourceCount());

			const FrameUploadArena::Stats uploadStats = dxCommon_->GetFrameUploadArena().GetStats();
			constexpr double kBytesPerMiB = 1024.0 * 1024.0;
			ImGui::Text("Frame Upload Arena: %.3f / %.3f MiB",
				static_cast<double>(uploadStats.usedBytes) / kBytesPerMiB,
				static_cast<double>(uploadStats.capacityBytes) / kBytesPerMiB);
			ImGui::Text("Upload High Water: %.3f MiB",
				static_cast<double>(uploadStats.highWaterBytes) / kBytesPerMiB);
			ImGui::Text("Upload Overflow: %.3f MiB (%zu allocs)",
				static_cast<double>(uploadStats.overflowBytes) / kBytesPerMiB,
				uploadStats.overflowAllocationCount); // Arena容量不足をPerformance画面から即座に判定できるようにする。
			if (uploadStats.overflowAllocationCount > 0)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.25f, 1.0f), "Frame Upload Arena overflow detected.");
			}

			const uint32_t backBufferIndex = dxCommon_->GetSwapChain()->GetSwapChain()->GetCurrentBackBufferIndex();
			const uint32_t commandFrameIndex = dxCommon_->GetCommandManager()->GetCurrentFrameIndex();
			const uint32_t arenaFrameIndex = uploadStats.currentFrameIndex;
			const bool frameIndicesMatch = backBufferIndex == commandFrameIndex && commandFrameIndex == arenaFrameIndex;
			if (dxCommon_->IsFramesInFlightActive())
			{
				if (frameIndicesMatch)
				{
					++framesInFlightStableFrames_;
				}
				else
				{
					++frameSyncMismatchCount_;
					framesInFlightStableFrames_ = 0;
				}
			}

			ImGui::SeparatorText("Frames in Flight Stability");
			ImGui::Text("BackBuffer / Command / Arena: %u / %u / %u", backBufferIndex, commandFrameIndex, arenaFrameIndex);
			ImGui::Text("Stable Frames: %llu", static_cast<unsigned long long>(framesInFlightStableFrames_));
			ImGui::Text("Index Mismatches: %llu", static_cast<unsigned long long>(frameSyncMismatchCount_));
			if (ImGui::Button("同期統計リセット"))
			{
				framesInFlightStableFrames_ = 0;
				frameSyncMismatchCount_ = 0;
			}
			if (!frameIndicesMatch)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.2f, 1.0f), "Frame Resource index mismatch detected.");
			}

			if (dxCommon_->IsFramesInFlightActive())
			{
				ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.45f, 1.0f), "Frames in Flight実行中: 長時間Stress Testで同期エラーを監視します。");
			}
			else
			{
				ImGui::TextDisabled("安全互換モード: 毎フレームGPU完了待ちを維持しています。");
			}
		}

		ImGui::SeparatorText("前回完了フレーム");
		ImGui::Text("Frame Interval: %.2f ms", frameTimingSummary_.frameIntervalMs);
		ImGui::Text("Update: %.3f ms", frameTimingSummary_.updateMs);
		ImGui::Text("Draw: %.3f ms", frameTimingSummary_.drawMs);
		ImGui::Text("EndDraw / Present Block: %.3f ms", frameTimingSummary_.presentMs);
		ImGui::Text("Total Frame: %.3f ms", frameTimingSummary_.totalFrameMs);

		const float accountedMs = frameTimingSummary_.updateMs + frameTimingSummary_.drawMs + frameTimingSummary_.presentMs;
		ImGui::Text("Unaccounted: %.3f ms", (std::max)(0.0f, frameTimingSummary_.frameIntervalMs - accountedMs));

		DirectXCommon::EndDrawPerformanceTiming endDrawTiming{};
		if (dxCommon_) endDrawTiming = dxCommon_->GetEndDrawPerformanceTiming();

		ImGui::SeparatorText("EndDraw CPU Detail");
		if (ImGui::BeginTable("##EndDrawPerformanceTable", 2,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableSetupColumn("Phase");
			ImGui::TableSetupColumn("Last ms");
			ImGui::TableHeadersRow();

			auto drawEndDrawRow = [](const char* label, float value)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(label);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.3f", value);
				};

			drawEndDrawRow("RenderTarget End", endDrawTiming.renderTargetEndMs);
			drawEndDrawRow("Transition To Present", endDrawTiming.transitionToPresentMs);
			drawEndDrawRow("CommandList Close", endDrawTiming.commandListCloseMs);
			drawEndDrawRow("ExecuteCommandLists", endDrawTiming.executeCommandListsMs);
			drawEndDrawRow("SwapChain Present", endDrawTiming.presentMs);
			drawEndDrawRow("Fence Signal", endDrawTiming.fenceSignalMs);
			drawEndDrawRow("Fence Wait", endDrawTiming.fenceWaitMs);
			drawEndDrawRow("Allocator Reset", endDrawTiming.allocatorResetMs);
			drawEndDrawRow("CommandList Reset", endDrawTiming.commandListResetMs);
			drawEndDrawRow("EndDraw Total", endDrawTiming.totalMs);
			ImGui::EndTable();
		}

		const RenderGraph::CompileStats& graphStats = renderGraph_.GetCompileStats();
		ImGui::SeparatorText("Render Graph");
		ImGui::Text("Render Graph Passes: %zu declared / %zu executed / %zu culled",
			graphStats.passCount, graphStats.executedPassCount, graphStats.culledPassCount);
		ImGui::Text("Culling Roots: %zu outputs / %zu side effects",
			graphStats.outputResourceCount, graphStats.sideEffectPassCount); // Pass Cullingの効き具合をPerformance画面から直接確認できるようにする。
		ImGui::Text("Resources: %zu / Dependencies: %zu", graphStats.resourceCount, graphStats.dependencyCount);
		ImGui::Text("Transient Logical Resources: %zu", graphStats.transientResourceCount);

		ImGui::SeparatorText("Render Pipeline CPU Pass");
		if (ImGui::BeginTable("##RenderPipelinePerformanceTable", 4,
			ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableSetupColumn("Phase");
			ImGui::TableSetupColumn("Last ms");
			ImGui::TableSetupColumn("EMA ms");
			ImGui::TableSetupColumn("Max ms");
			ImGui::TableHeadersRow();

			for (std::size_t index = 0; index < kPerformancePhaseCount; ++index)
			{
				const PerformancePhase phase = static_cast<PerformancePhase>(index);
				const PerformanceMetric& metric = performanceMetrics_[index];
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(GetPerformancePhaseName(phase));
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", metric.lastMs);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f", metric.averageMs);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.3f", metric.maxMs);
			}
			ImGui::EndTable();
		}

		ImGui::SeparatorText("Render Pipeline GPU Timestamp");
		if (!gpuTimingAvailable_)
		{
			ImGui::TextDisabled("GPU Timestamp Query is unavailable.");
		}
		else
		{
			ImGui::Text("GPU Pipeline Total: Last %.3f ms / EMA %.3f ms / Max %.3f ms",
				gpuFrameMetric_.lastMs, gpuFrameMetric_.averageMs, gpuFrameMetric_.maxMs);
			if (ImGui::BeginTable("##RenderPipelineGpuPerformanceTable", 4,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableSetupColumn("Phase");
				ImGui::TableSetupColumn("Last ms");
				ImGui::TableSetupColumn("EMA ms");
				ImGui::TableSetupColumn("Max ms");
				ImGui::TableHeadersRow();
				for (std::size_t index = 0; index < kPerformancePhaseCount; ++index)
				{
					const PerformancePhase phase = static_cast<PerformancePhase>(index);
					const PerformanceMetric& metric = gpuPerformanceMetrics_[index];
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(GetPerformancePhaseName(phase));
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.3f", metric.lastMs);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%.3f", metric.averageMs);
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%.3f", metric.maxMs);
				}
				ImGui::EndTable();
			}
		}

		ImGui::SeparatorText("判定");
		const float editorUiMs = GetPerformanceMetric(PerformancePhase::EditorUiBuild).averageMs;
		const float mainWorldMs = GetPerformanceMetric(PerformancePhase::MainWorldRender).averageMs;
		const float shadowMs = GetPerformanceMetric(PerformancePhase::ShadowRender).averageMs;
		const float gpuFrameMs = gpuFrameMetric_.averageMs;

		if (endDrawTiming.fenceWaitMs > 4.0f && gpuFrameMetric_.sampleCount > 0 && gpuFrameMs < 4.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "GPU実処理よりFence Waitが大きいため、VSync / Frame pacing / Resource再利用待ちを優先して調査します。");
		}
		else if (gpuFrameMetric_.sampleCount > 0 && gpuFrameMs > 8.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "GPU Pipelineが重いです。GPU Timestamp表の上位Passを優先して調査します。");
		}
		else if (endDrawTiming.presentMs > 4.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "SwapChain Presentが大きいです。VSyncとSwapChain設定を優先して調査します。");
		}
		else if (editorUiMs > 8.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Editor UI Buildが重いです。Outliner/Inspectorの構築処理を優先して調査します。");
		}
		else if (mainWorldMs + shadowMs > 8.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "Main World + ShadowのCPU記録が重いです。DrawCallとMesh描画回数を優先して調査します。");
		}
		else
		{
			ImGui::TextDisabled("CPU/GPUとも大きな単発ボトルネックは検出していません。");
		}

		ImGui::End();
#endif // USE_IMGUI
	}

	void RenderPipelineController::ExecuteShadowMapPass(const FrameCallbacks& callbacks)
	{
		MeasurePhase(PerformancePhase::ShadowPrepare, callbacks.prepareShadowPass);
		MeasurePhase(PerformancePhase::ShadowRender, [&callbacks]()
			{
				LightManager::GetInstance()->ExecuteShadowPasses(callbacks.drawShadowObjects);
			});
	}

	void RenderPipelineController::ExecuteEditorFrame(const FrameCallbacks& callbacks)
	{
		MeasurePhase(PerformancePhase::EditorUiBuild, callbacks.buildEditorUi);
		MeasurePhase(PerformancePhase::EditorPicking, callbacks.executeEditorPickingPass);
		MeasurePhase(PerformancePhase::MainWorldRender, callbacks.drawGameWorldToSceneTarget);
		MeasurePhase(PerformancePhase::PostEffect, callbacks.renderPostEffectToGameRenderTarget);
		MeasurePhase(PerformancePhase::SelectionOutline, callbacks.renderEditorSelectionOutline);
		MeasurePhase(PerformancePhase::SceneOverlay, [&callbacks]()
			{
				if (callbacks.beginGameRenderTargetOverlay) callbacks.beginGameRenderTargetOverlay();
				if (callbacks.drawScene2DOverlay) callbacks.drawScene2DOverlay();
				if (callbacks.endGameRenderTargetOverlay) callbacks.endGameRenderTargetOverlay();
			});
		MeasurePhase(PerformancePhase::ImGuiRender, callbacks.drawImGuiOverlay);
	}

	void RenderPipelineController::ExecuteGameFrame(const FrameCallbacks& callbacks)
	{
		MeasurePhase(PerformancePhase::MainWorldRender, callbacks.drawGameWorldToSceneTarget);
		MeasurePhase(PerformancePhase::BackBufferPostEffect, callbacks.applyPostEffectToBackBuffer);
		MeasurePhase(PerformancePhase::BackBufferRebind, callbacks.rebindBackBufferForGameOverlay);
		MeasurePhase(PerformancePhase::GameUi, callbacks.drawGameUIToBackBuffer);
	}

} // namespace Ken4lowEngine