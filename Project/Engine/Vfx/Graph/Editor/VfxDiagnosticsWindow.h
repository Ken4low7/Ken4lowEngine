#pragma once

#include "Engine/Vfx/Graph/Diagnostics/VfxGraphDiagnostics.h"

#include <algorithm>
#include <array>
#include <cstdio>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{

/// <summary>
/// Phase28 companion diagnostics window for the VFX Graph editor.
/// </summary>
class VfxDiagnosticsWindow
{
public:
	static VfxDiagnosticsWindow* GetInstance()
	{
		static VfxDiagnosticsWindow instance;
		return &instance;
	}

	void Draw(bool visible)
	{
#ifdef USE_IMGUI
		if (!visible) return;
		if (!ImGui::Begin("VFX Diagnostics##Phase28"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::BeginTabBar("VfxDiagnosticsTabs"))
		{
			if (ImGui::BeginTabItem("Overview"))
			{
				DrawOverview();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("History"))
			{
				DrawHistory();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Stress"))
			{
				DrawStress();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Budget"))
			{
				DrawBudget();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
#else
		(void)visible;
#endif // USE_IMGUI
	}

private:
	VfxDiagnosticsWindow()
	{
		std::snprintf(graphName_.data(), graphName_.size(), "%s", "Phase27ScalableIntegratedExplosion");
	}

#ifdef USE_IMGUI
	void DrawOverview()
	{
		VfxGraphDiagnostics* diagnostics = VfxGraphDiagnostics::GetInstance();
		const VfxDiagnosticsFrameSample* sample = diagnostics->GetLatestSample();
		const VfxDiagnosticsSummary summary = diagnostics->BuildSummary();
		if (sample == nullptr)
		{
			ImGui::TextUnformatted("No frame samples captured yet.");
			return;
		}

		ImGui::Text("Frame #%llu", static_cast<unsigned long long>(sample->frameNumber));
		ImGui::Text("Frame %.3f ms | Update %.3f | Draw %.3f | Present %.3f",
			sample->totalFrameMs, sample->updateMs, sample->drawMs, sample->presentMs);
		ImGui::Separator();
		ImGui::Text("Particles est. %u | Emitters %u active / %u | Draw calls %u | Emit dispatch %llu",
			sample->estimatedActiveParticles,
			sample->activeEmitterCount,
			sample->emitterCount,
			sample->particleDrawCalls,
			static_cast<unsigned long long>(sample->emitDispatchesThisFrame));
		ImGui::Text("Graph loops %u | Active cost %u | Start cost %u | Culls %llu | Budget rejects %llu",
			sample->graphActiveLoops,
			sample->graphActiveLoopCost,
			sample->graphStartCostThisFrame,
			static_cast<unsigned long long>(sample->graphCullsThisFrame),
			static_cast<unsigned long long>(sample->graphBudgetRejectsThisFrame));
		ImGui::Text("Cue instances %u | Tracks %u | Starts %llu | Delays %llu",
			sample->cueActiveInstances,
			sample->cueActiveTracks,
			static_cast<unsigned long long>(sample->cueTrackStartsThisFrame),
			static_cast<unsigned long long>(sample->cueBudgetDelaysThisFrame));
		ImGui::Separator();
		ImGui::Text("%u-sample avg %.3f ms | max %.3f ms", summary.sampleCount, summary.averageFrameMs, summary.maxFrameMs);
		ImGui::Text("Peak particles %u | particle draws %u | graph loops %u | cue tracks %u",
			summary.peakEstimatedActiveParticles,
			summary.peakParticleDrawCalls,
			summary.peakGraphActiveLoops,
			summary.peakCueActiveTracks);
		ImGui::TextDisabled("Phase28 intentionally samples CPU-visible counters only; no GPU fence wait/readback is performed.");
	}

	void DrawHistory()
	{
		VfxGraphDiagnostics* diagnostics = VfxGraphDiagnostics::GetInstance();
		const std::size_t count = diagnostics->GetSampleCount();
		std::array<float, VfxGraphDiagnostics::kHistoryCapacity> frame{};
		std::array<float, VfxGraphDiagnostics::kHistoryCapacity> update{};
		std::array<float, VfxGraphDiagnostics::kHistoryCapacity> draw{};
		std::array<float, VfxGraphDiagnostics::kHistoryCapacity> particles{};
		for (std::size_t index = 0u; index < count; ++index)
		{
			const VfxDiagnosticsFrameSample* sample = diagnostics->GetSampleFromOldest(index);
			if (sample == nullptr) continue;
			frame[index] = sample->totalFrameMs;
			update[index] = sample->updateMs;
			draw[index] = sample->drawMs;
			particles[index] = static_cast<float>(sample->estimatedActiveParticles);
		}

		ImGui::Text("History: %zu / %zu frames", count, VfxGraphDiagnostics::kHistoryCapacity);
		if (count > 0u)
		{
			ImGui::PlotLines("Frame ms", frame.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 80.0f));
			ImGui::PlotLines("Update ms", update.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
			ImGui::PlotLines("Draw ms", draw.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
			ImGui::PlotLines("Estimated particles", particles.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
		}
		if (ImGui::Button("Reset History")) diagnostics->ResetHistory();
	}

	void DrawStress()
	{
		VfxGraphDiagnostics* diagnostics = VfxGraphDiagnostics::GetInstance();
		ImGui::InputText("Graph", graphName_.data(), graphName_.size());
		if (ImGui::Button("Load Phase27 Sample"))
		{
			const bool loaded = VfxGraphRuntime::GetInstance()->LoadGraph("Resources/VfxGraph/Phase27/ScalableIntegratedExplosion.vfxgraph.json");
			lastStressMessage_ = loaded ? "Phase27 stress sample loaded." : VfxGraphRuntime::GetInstance()->GetLastStatus();
		}

		ImGui::InputInt("One-shots", &oneShotCount_);
		ImGui::InputInt("Loops", &loopCount_);
		ImGui::InputInt("Grid columns", &gridColumns_);
		ImGui::DragFloat("Spacing", &spacing_, 0.1f, 0.1f, 50.0f);
		oneShotCount_ = (std::clamp)(oneShotCount_, 0, static_cast<int>(VfxGraphDiagnostics::kMaxStressOneShots));
		loopCount_ = (std::clamp)(loopCount_, 0, static_cast<int>(VfxGraphDiagnostics::kMaxStressLoops));
		gridColumns_ = (std::clamp)(gridColumns_, 1, 64);

		if (ImGui::Button("Run Stress Burst"))
		{
			VfxGraphStressConfig config{};
			config.graphName = graphName_.data();
			config.oneShotCount = static_cast<uint32_t>(oneShotCount_);
			config.loopCount = static_cast<uint32_t>(loopCount_);
			config.gridColumns = static_cast<uint32_t>(gridColumns_);
			config.spacing = spacing_;
			config.center = stressCenter_;
			const bool success = diagnostics->RunStress(config);
			lastStressMessage_ = success ? "Stress burst submitted." : "Stress burst produced no successful starts. Check registration, budget, distance, and culling.";
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop Stress Loops"))
		{
			const uint32_t stopped = diagnostics->StopStressLoops();
			lastStressMessage_ = "Stopped stress loops: " + std::to_string(stopped);
		}

		const VfxGraphStressResult& result = diagnostics->GetLastStressResult();
		ImGui::Text("One-shots %u / %u | Loops %u / %u | Active stress loops %zu",
			result.successfulOneShots,
			result.requestedOneShots,
			result.successfulLoops,
			result.requestedLoops,
			diagnostics->GetActiveStressLoopCount());
		ImGui::Text("Budget rejects %llu | Culls %llu | Est. particles %u | Active emitters %u",
			static_cast<unsigned long long>(result.graphBudgetRejects),
			static_cast<unsigned long long>(result.graphCulls),
			result.estimatedActiveParticlesAfterStart,
			result.activeEmittersAfterStart);
		ImGui::TextWrapped("%s", lastStressMessage_.c_str());
	}

	void DrawBudget()
	{
		VfxRuntimeBudget& budget = VfxCueRuntime::GetInstance()->GetEditableBudget();
		int graphStartCost = static_cast<int>(budget.maxVfxGraphStartCostPerFrame);
		int activeGraphCost = static_cast<int>(budget.maxActiveVfxGraphLoopCost);
		int activeInstances = static_cast<int>(budget.maxActiveInstances);
		int activeTracks = static_cast<int>(budget.maxActiveTracks);
		int trackStarts = static_cast<int>(budget.maxTrackStartsPerFrame);

		if (ImGui::InputInt("Graph start cost / frame", &graphStartCost))
			budget.maxVfxGraphStartCostPerFrame = static_cast<uint32_t>((std::max)(graphStartCost, 1));
		if (ImGui::InputInt("Active graph loop cost", &activeGraphCost))
			budget.maxActiveVfxGraphLoopCost = static_cast<uint32_t>((std::max)(activeGraphCost, 1));
		if (ImGui::InputInt("Active cue instances", &activeInstances))
			budget.maxActiveInstances = static_cast<uint32_t>((std::max)(activeInstances, 1));
		if (ImGui::InputInt("Active cue tracks", &activeTracks))
			budget.maxActiveTracks = static_cast<uint32_t>((std::max)(activeTracks, 1));
		if (ImGui::InputInt("Track starts / frame", &trackStarts))
			budget.maxTrackStartsPerFrame = static_cast<uint32_t>((std::max)(trackStarts, 1));

		ImGui::Separator();
		const VfxGraphRuntimeStats& graph = VfxGraphRuntime::GetInstance()->GetStats();
		const VfxRuntimeStats& cue = VfxCueRuntime::GetInstance()->GetStats();
		ImGui::Text("Current graph cost: %u active / %u starts this frame", graph.activeLoopCost, graph.graphStartCostThisFrame);
		ImGui::Text("Current cues: %u instances / %u tracks / %u starts this frame",
			cue.activeInstanceCount, cue.activeTrackCount, cue.trackStartsThisFrame);
	}
#endif // USE_IMGUI

	std::array<char, 128> graphName_{};
	int oneShotCount_ = 32;
	int loopCount_ = 8;
	int gridColumns_ = 8;
	float spacing_ = 2.0f;
	Vector3 stressCenter_{ 0.0f, 1.0f, 0.0f };
	std::string lastStressMessage_ = "Ready.";
};

} // namespace Ken4lowEngine
