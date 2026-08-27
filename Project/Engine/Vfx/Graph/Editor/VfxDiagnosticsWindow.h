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
/// VFX Graphの実行状況、履歴、負荷確認、Budgetをまとめて表示します。
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
		if (!ImGui::Begin("VFX診断##VfxDiagnostics"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::BeginTabBar("VfxDiagnosticsTabs"))
		{
			if (ImGui::BeginTabItem("概要"))
			{
				DrawOverview();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("履歴"))
			{
				DrawHistory();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("負荷確認"))
			{
				DrawStress();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("実行予算"))
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
			ImGui::TextUnformatted("まだフレーム情報が記録されていません。");
			return;
		}

		ImGui::Text("フレーム #%llu", static_cast<unsigned long long>(sample->frameNumber));
		ImGui::Text("Frame %.3f ms | Update %.3f | Draw %.3f | Present %.3f",
			sample->totalFrameMs, sample->updateMs, sample->drawMs, sample->presentMs);
		ImGui::Separator();
		ImGui::Text("推定粒子数 %u | Emitter %u 使用中 / %u | Draw Call %u | Emit Dispatch %llu",
			sample->estimatedActiveParticles,
			sample->activeEmitterCount,
			sample->emitterCount,
			sample->particleDrawCalls,
			static_cast<unsigned long long>(sample->emitDispatchesThisFrame));
		ImGui::Text("Graph Loop %u | 使用Cost %u | 開始Cost %u | Culling %llu | Budget拒否 %llu",
			sample->graphActiveLoops,
			sample->graphActiveLoopCost,
			sample->graphStartCostThisFrame,
			static_cast<unsigned long long>(sample->graphCullsThisFrame),
			static_cast<unsigned long long>(sample->graphBudgetRejectsThisFrame));
		ImGui::Text("Cue Instance %u | Track %u | 開始 %llu | 遅延 %llu",
			sample->cueActiveInstances,
			sample->cueActiveTracks,
			static_cast<unsigned long long>(sample->cueTrackStartsThisFrame),
			static_cast<unsigned long long>(sample->cueBudgetDelaysThisFrame));
		ImGui::Separator();
		ImGui::Text("%uサンプル平均 %.3f ms | 最大 %.3f ms", summary.sampleCount, summary.averageFrameMs, summary.maxFrameMs);
		ImGui::Text("最大粒子数 %u | Particle Draw %u | Graph Loop %u | Cue Track %u",
			summary.peakEstimatedActiveParticles,
			summary.peakParticleDrawCalls,
			summary.peakGraphActiveLoops,
			summary.peakCueActiveTracks);
		ImGui::TextDisabled("CPUから取得できる統計だけを記録し、GPU Fence待機やReadbackは行いません。");
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

		ImGui::Text("履歴: %zu / %zu フレーム", count, VfxGraphDiagnostics::kHistoryCapacity);
		if (count > 0u)
		{
			ImGui::PlotLines("Frame ms", frame.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 80.0f));
			ImGui::PlotLines("Update ms", update.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
			ImGui::PlotLines("Draw ms", draw.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
			ImGui::PlotLines("推定粒子数", particles.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
		}
		if (ImGui::Button("履歴をリセット")) diagnostics->ResetHistory();
	}

	void DrawStress()
	{
		VfxGraphDiagnostics* diagnostics = VfxGraphDiagnostics::GetInstance();
		ImGui::InputText("Graph名", graphName_.data(), graphName_.size());
		if (ImGui::Button("負荷確認サンプルを読み込む"))
		{
			const bool loaded = VfxGraphRuntime::GetInstance()->LoadGraph("Resources/VfxGraph/Phase27/ScalableIntegratedExplosion.vfxgraph.json");
			lastStressMessage_ = loaded ? "負荷確認用Graphを読み込みました。" : VfxGraphRuntime::GetInstance()->GetLastStatus();
		}

		ImGui::InputInt("単発数", &oneShotCount_);
		ImGui::InputInt("Loop数", &loopCount_);
		ImGui::InputInt("Grid列数", &gridColumns_);
		ImGui::DragFloat("間隔", &spacing_, 0.1f, 0.1f, 50.0f);
		oneShotCount_ = (std::clamp)(oneShotCount_, 0, static_cast<int>(VfxGraphDiagnostics::kMaxStressOneShots));
		loopCount_ = (std::clamp)(loopCount_, 0, static_cast<int>(VfxGraphDiagnostics::kMaxStressLoops));
		gridColumns_ = (std::clamp)(gridColumns_, 1, 64);

		if (ImGui::Button("負荷確認を実行"))
		{
			VfxGraphStressConfig config{};
			config.graphName = graphName_.data();
			config.oneShotCount = static_cast<uint32_t>(oneShotCount_);
			config.loopCount = static_cast<uint32_t>(loopCount_);
			config.gridColumns = static_cast<uint32_t>(gridColumns_);
			config.spacing = spacing_;
			config.center = stressCenter_;
			const bool success = diagnostics->RunStress(config);
			lastStressMessage_ = success ? "負荷確認を開始しました。" : "開始できませんでした。登録状態、Budget、距離、Culling設定を確認してください。";
		}
		ImGui::SameLine();
		if (ImGui::Button("負荷確認Loopを停止"))
		{
			const uint32_t stopped = diagnostics->StopStressLoops();
			lastStressMessage_ = "停止したLoop数: " + std::to_string(stopped);
		}

		const VfxGraphStressResult& result = diagnostics->GetLastStressResult();
		ImGui::Text("単発 %u / %u | Loop %u / %u | 実行中Loop %zu",
			result.successfulOneShots,
			result.requestedOneShots,
			result.successfulLoops,
			result.requestedLoops,
			diagnostics->GetActiveStressLoopCount());
		ImGui::Text("Budget拒否 %llu | Culling %llu | 推定粒子数 %u | Active Emitter %u",
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

		if (ImGui::InputInt("1フレームのGraph開始Cost", &graphStartCost))
			budget.maxVfxGraphStartCostPerFrame = static_cast<uint32_t>((std::max)(graphStartCost, 1));
		if (ImGui::InputInt("Active Graph Loop Cost", &activeGraphCost))
			budget.maxActiveVfxGraphLoopCost = static_cast<uint32_t>((std::max)(activeGraphCost, 1));
		if (ImGui::InputInt("Active Cue Instance", &activeInstances))
			budget.maxActiveInstances = static_cast<uint32_t>((std::max)(activeInstances, 1));
		if (ImGui::InputInt("Active Cue Track", &activeTracks))
			budget.maxActiveTracks = static_cast<uint32_t>((std::max)(activeTracks, 1));
		if (ImGui::InputInt("1フレームのTrack開始数", &trackStarts))
			budget.maxTrackStartsPerFrame = static_cast<uint32_t>((std::max)(trackStarts, 1));

		ImGui::Separator();
		const VfxGraphRuntimeStats& graph = VfxGraphRuntime::GetInstance()->GetStats();
		const VfxRuntimeStats& cue = VfxCueRuntime::GetInstance()->GetStats();
		ImGui::Text("現在のGraph Cost: Active %u / このフレームの開始 %u", graph.activeLoopCost, graph.graphStartCostThisFrame);
		ImGui::Text("現在のCue: Instance %u / Track %u / このフレームの開始 %u",
			cue.activeInstanceCount, cue.activeTrackCount, cue.trackStartsThisFrame); // 表示名だけ日本語化し、実行Budgetの内部単位は変更しない。
	}
#endif // USE_IMGUI

	std::array<char, 128> graphName_{};
	int oneShotCount_ = 32;
	int loopCount_ = 8;
	int gridColumns_ = 8;
	float spacing_ = 2.0f;
	Vector3 stressCenter_{ 0.0f, 1.0f, 0.0f };
	std::string lastStressMessage_ = "準備完了";
};

} // namespace Ken4lowEngine
