#pragma once

#include <ActorWorld.h>
#include <GameTimer.h>
#include <PhysicsWorld.h>
#include <RenderPipelineController.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

class PerformancePhaseValidation
{
public:
	PerformancePhaseValidation(
		Ken4lowEngine::ActorWorld* actorWorld = nullptr,
		Ken4lowEngine::PhysicsWorld* physicsWorld = nullptr)
		: actorWorld_(actorWorld), physicsWorld_(physicsWorld)
	{
	}

	enum class Phase : std::size_t
	{
		ActorWorldUpdate,
		PhysicsWorldUpdate,
		PostPhysicsUpdate,
		ActorDraw,
		PhysicsDebugDraw,
		ShadowDraw,
		ScreenSpaceUI,
		Count,
	};

	void BeginFrame(float deltaTime)
	{
		frameIntervalMs_ = (std::max)(0.0f, deltaTime * 1000.0f); // GameTimerのDeltaTimeを実フレーム間隔として記録する。
		instantFps_ = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
	}

	void SetPhysicsState(
		std::size_t actorCount,
		std::size_t colliderCount,
		std::size_t contactCount,
		int subStepCount)
	{
		actorCount_ = actorCount;
		colliderCount_ = colliderCount;
		contactCount_ = contactCount;
		subStepCount_ = subStepCount;
		theoreticalPairCount_ = colliderCount > 1
			? (colliderCount * (colliderCount - 1)) / 2
			: 0;
	}

	void BeginPhase(Phase phase)
	{
		phaseBegin_[ToIndex(phase)] = Clock::now();
	}

	void EndPhase(Phase phase)
	{
		const std::size_t index = ToIndex(phase);
		const float elapsedMs = std::chrono::duration<float, std::milli>(Clock::now() - phaseBegin_[index]).count();
		Metric& metric = metrics_[index];
		metric.lastMs = (std::max)(0.0f, elapsedMs);
		metric.averageMs = metric.sampleCount == 0
			? metric.lastMs
			: metric.averageMs * 0.9f + metric.lastMs * 0.1f;
		metric.maxMs = (std::max)(metric.maxMs, metric.lastMs);
		++metric.sampleCount;
	}

	void DrawImGui()
	{
#ifdef USE_IMGUI
		RefreshWorldState(); // Editor停止中でもActor/Collider数を現在のWorldから直接取得する。

		const Ken4lowEngine::GameTimer::CompletedFrameTiming& completed = Ken4lowEngine::GameTimer::GetInstance()->GetCompletedFrameTiming();
		const float displayFrameIntervalMs = completed.frameIntervalMs > 0.0f ? completed.frameIntervalMs : frameIntervalMs_;
		const float displayInstantFps = displayFrameIntervalMs > 0.0f ? 1000.0f / displayFrameIntervalMs : instantFps_;

		if (!ImGui::Begin("Performance Phase 検証"))
		{
			ImGui::End();
			return;
		}

		if (ImGui::Button("最大値リセット"))
		{
			for (Metric& metric : metrics_) metric.maxMs = metric.lastMs;
		}

		ImGui::SeparatorText("実フレーム");
		ImGui::Text("Frame Interval: %.2f ms", displayFrameIntervalMs);
		ImGui::Text("Instant FPS: %.1f", displayInstantFps);

		const float measuredSceneMs =
			GetMetric(Phase::ActorWorldUpdate).lastMs +
			GetMetric(Phase::PhysicsWorldUpdate).lastMs +
			GetMetric(Phase::PostPhysicsUpdate).lastMs +
			GetMetric(Phase::ActorDraw).lastMs +
			GetMetric(Phase::PhysicsDebugDraw).lastMs +
			GetMetric(Phase::ShadowDraw).lastMs +
			GetMetric(Phase::ScreenSpaceUI).lastMs;
		ImGui::Text("Measured DebugScene CPU: %.2f ms", measuredSceneMs);
		ImGui::Text("Unmeasured / Other: %.2f ms", (std::max)(0.0f, displayFrameIntervalMs - measuredSceneMs));
		ImGui::TextDisabled("OtherにはEditor UI、PostEffect、GPU待ち、Present、Framework共通処理などが含まれます。");

		ImGui::SeparatorText("Physics State");
		ImGui::Text("Actor Count: %zu", actorCount_);
		ImGui::Text("Collider Count: %zu", colliderCount_);
		ImGui::Text("Theoretical Pair Count: %zu", theoreticalPairCount_);
		ImGui::Text("Contact Count: %zu", contactCount_);
		ImGui::Text("Sub Step Count: %d", subStepCount_);

		ImGui::SeparatorText("CPU Phase");
		if (ImGui::BeginTable("##PerformancePhaseTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableSetupColumn("Phase");
			ImGui::TableSetupColumn("Last ms");
			ImGui::TableSetupColumn("EMA ms");
			ImGui::TableSetupColumn("Max ms");
			ImGui::TableHeadersRow();
			DrawMetricRow("ActorWorld Update", Phase::ActorWorldUpdate);
			DrawMetricRow("PhysicsWorld Update", Phase::PhysicsWorldUpdate);
			DrawMetricRow("PostPhysics Update", Phase::PostPhysicsUpdate);
			DrawMetricRow("Actor Draw", Phase::ActorDraw);
			DrawMetricRow("Physics Debug Draw", Phase::PhysicsDebugDraw);
			DrawMetricRow("Shadow Draw", Phase::ShadowDraw);
			DrawMetricRow("Screen Space UI", Phase::ScreenSpaceUI);
			ImGui::EndTable();
		}

		ImGui::SeparatorText("次の最適化判断");
		if (GetMetric(Phase::PhysicsWorldUpdate).averageMs > 4.0f)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.72f, 0.25f, 1.0f), "PhysicsWorldが重いため、Static Spatial Gridの導入効果を計測します。");
		}
		else
		{
			ImGui::TextDisabled("PhysicsWorldが軽い場合は、Other側のEditor/Render/GPU待ちを優先して調査します。");
		}

		ImGui::End();

		if (Ken4lowEngine::RenderPipelineController* renderPipeline = Ken4lowEngine::RenderPipelineController::GetActiveController())
		{
			renderPipeline->DrawPerformanceImGui(); // 同じ診断タイミングで描画PassとPresent待ちも別ウィンドウへ表示する。
		}
#endif // USE_IMGUI
	}

private:
	using Clock = std::chrono::steady_clock;

	struct Metric
	{
		float lastMs = 0.0f;
		float averageMs = 0.0f;
		float maxMs = 0.0f;
		std::size_t sampleCount = 0;
	};

	static constexpr std::size_t kPhaseCount = static_cast<std::size_t>(Phase::Count);

	static constexpr std::size_t ToIndex(Phase phase)
	{
		return static_cast<std::size_t>(phase);
	}

	void RefreshWorldState()
	{
		if (!actorWorld_ || !physicsWorld_) return;
		SetPhysicsState(
			actorWorld_->GetActors().size(),
			physicsWorld_->GetColliderCount(),
			physicsWorld_->GetContactCount(),
			physicsWorld_->GetLastSubStepCount());
	}

	const Metric& GetMetric(Phase phase) const
	{
		return metrics_[ToIndex(phase)];
	}

#ifdef USE_IMGUI
	void DrawMetricRow(const char* label, Phase phase) const
	{
		const Metric& metric = GetMetric(phase);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		ImGui::Text("%.3f", metric.lastMs);
		ImGui::TableSetColumnIndex(2);
		ImGui::Text("%.3f", metric.averageMs);
		ImGui::TableSetColumnIndex(3);
		ImGui::Text("%.3f", metric.maxMs);
	}
#endif // USE_IMGUI

private:
	Ken4lowEngine::ActorWorld* actorWorld_ = nullptr;
	Ken4lowEngine::PhysicsWorld* physicsWorld_ = nullptr;
	std::array<Metric, kPhaseCount> metrics_{};
	std::array<Clock::time_point, kPhaseCount> phaseBegin_{};
	float frameIntervalMs_ = 0.0f;
	float instantFps_ = 0.0f;
	std::size_t actorCount_ = 0;
	std::size_t colliderCount_ = 0;
	std::size_t theoreticalPairCount_ = 0;
	std::size_t contactCount_ = 0;
	int subStepCount_ = 0;
};
