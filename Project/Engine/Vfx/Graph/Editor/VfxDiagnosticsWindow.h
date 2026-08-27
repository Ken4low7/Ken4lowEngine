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
/// VFXグラフの実行状況、履歴、負荷確認、実行予算をまとめて表示します。
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
		std::snprintf(graphName_.data(), graphName_.size(), "%s", "ScalableIntegratedExplosion");
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
		ImGui::Text("フレーム %.3f ms | 更新 %.3f | 描画 %.3f | 表示 %.3f",
			sample->totalFrameMs, sample->updateMs, sample->drawMs, sample->presentMs);
		ImGui::Separator();
		ImGui::Text("推定粒子数 %u | エミッター %u 使用中 / %u | 描画呼出 %u | 放出ディスパッチ %llu",
			sample->estimatedActiveParticles,
			sample->activeEmitterCount,
			sample->emitterCount,
			sample->particleDrawCalls,
			static_cast<unsigned long long>(sample->emitDispatchesThisFrame));
		ImGui::Text("グラフループ %u | 使用コスト %u | 開始コスト %u | カリング %llu | 実行予算拒否 %llu",
			sample->graphActiveLoops,
			sample->graphActiveLoopCost,
			sample->graphStartCostThisFrame,
			static_cast<unsigned long long>(sample->graphCullsThisFrame),
			static_cast<unsigned long long>(sample->graphBudgetRejectsThisFrame));
		ImGui::Text("キュー実体 %u | トラック %u | 開始 %llu | 遅延 %llu",
			sample->cueActiveInstances,
			sample->cueActiveTracks,
			static_cast<unsigned long long>(sample->cueTrackStartsThisFrame),
			static_cast<unsigned long long>(sample->cueBudgetDelaysThisFrame));
		ImGui::Separator();
		ImGui::Text("%uサンプル平均 %.3f ms | 最大 %.3f ms", summary.sampleCount, summary.averageFrameMs, summary.maxFrameMs);
		ImGui::Text("最大粒子数 %u | 粒子描画 %u | グラフループ %u | キュートラック %u",
			summary.peakEstimatedActiveParticles,
			summary.peakParticleDrawCalls,
			summary.peakGraphActiveLoops,
			summary.peakCueActiveTracks);
		ImGui::TextDisabled("CPUから取得できる統計だけを記録し、GPUフェンス待機や読戻しは行いません。");
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
			ImGui::PlotLines("フレーム時間 (ms)", frame.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 80.0f));
			ImGui::PlotLines("更新時間 (ms)", update.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
			ImGui::PlotLines("描画時間 (ms)", draw.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
			ImGui::PlotLines("推定粒子数", particles.data(), static_cast<int>(count), 0, nullptr, 0.0f, FLT_MAX, ImVec2(0.0f, 70.0f));
		}
		if (ImGui::Button("履歴をリセット")) diagnostics->ResetHistory();
	}

	void DrawStress()
	{
		VfxGraphDiagnostics* diagnostics = VfxGraphDiagnostics::GetInstance();
		ImGui::InputText("グラフ名", graphName_.data(), graphName_.size());
		if (ImGui::Button("負荷確認サンプルを読み込む"))
		{
			const bool loaded = VfxGraphRuntime::GetInstance()->LoadGraph("Resources/VfxGraph/Samples/ScalableIntegratedExplosion.vfxgraph.json");
			lastStressMessage_ = loaded ? "負荷確認用グラフを読み込みました。" : VfxGraphRuntime::GetInstance()->GetLastStatus(); // 表示文だけ日本語化し、アセット識別子は変更しない。
		}

		ImGui::InputInt("単発数", &oneShotCount_);
		ImGui::InputInt("ループ数", &loopCount_);
		ImGui::InputInt("グリッド列数", &gridColumns_);
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
			lastStressMessage_ = success ? "負荷確認を開始しました。" : "開始できませんでした。登録状態、実行予算、距離、カリング設定を確認してください。";
		}
		ImGui::SameLine();
		if (ImGui::Button("負荷確認ループを停止"))
		{
			const uint32_t stopped = diagnostics->StopStressLoops();
			lastStressMessage_ = "停止したループ数: " + std::to_string(stopped);
		}

		const VfxGraphStressResult& result = diagnostics->GetLastStressResult();
		ImGui::Text("単発 %u / %u | ループ %u / %u | 実行中ループ %zu",
			result.successfulOneShots,
			result.requestedOneShots,
			result.successfulLoops,
			result.requestedLoops,
			diagnostics->GetActiveStressLoopCount());
		ImGui::Text("実行予算拒否 %llu | カリング %llu | 推定粒子数 %u | 使用中エミッター %u",
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

		if (ImGui::InputInt("1フレームのグラフ開始コスト", &graphStartCost))
			budget.maxVfxGraphStartCostPerFrame = static_cast<uint32_t>((std::max)(graphStartCost, 1));
		if (ImGui::InputInt("使用中グラフループコスト", &activeGraphCost))
			budget.maxActiveVfxGraphLoopCost = static_cast<uint32_t>((std::max)(activeGraphCost, 1));
		if (ImGui::InputInt("使用中キュー実体数", &activeInstances))
			budget.maxActiveInstances = static_cast<uint32_t>((std::max)(activeInstances, 1));
		if (ImGui::InputInt("使用中キュートラック数", &activeTracks))
			budget.maxActiveTracks = static_cast<uint32_t>((std::max)(activeTracks, 1));
		if (ImGui::InputInt("1フレームのトラック開始数", &trackStarts))
			budget.maxTrackStartsPerFrame = static_cast<uint32_t>((std::max)(trackStarts, 1));

		ImGui::Separator();
		const VfxGraphRuntimeStats& graph = VfxGraphRuntime::GetInstance()->GetStats();
		const VfxRuntimeStats& cue = VfxCueRuntime::GetInstance()->GetStats();
		ImGui::Text("現在のグラフコスト: 使用中 %u / このフレームの開始 %u", graph.activeLoopCost, graph.graphStartCostThisFrame);
		ImGui::Text("現在のキュー: 実体 %u / トラック %u / このフレームの開始 %u",
			cue.activeInstanceCount, cue.activeTrackCount, cue.trackStartsThisFrame);
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
