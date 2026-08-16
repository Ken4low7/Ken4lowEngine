#pragma once

#include "Engine/Core/Time/Core/GameTimer.h"
#include "Engine/Graphics/Renderer/GpuParticle/Manager/GpuParticleManager.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphRuntime.h"
#include "Engine/Vfx/Runtime/VfxCueRuntime.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace Ken4lowEngine
{

struct VfxDiagnosticsFrameSample
{
	uint64_t frameNumber = 0u;
	float frameIntervalMs = 0.0f;
	float updateMs = 0.0f;
	float drawMs = 0.0f;
	float presentMs = 0.0f;
	float totalFrameMs = 0.0f;

	uint32_t emitterCount = 0u;
	uint32_t activeEmitterCount = 0u;
	uint32_t estimatedActiveParticles = 0u;
	uint32_t particleDrawCalls = 0u;
	uint64_t emitDispatchesThisFrame = 0u;

	uint32_t graphActiveLoops = 0u;
	uint32_t graphActiveLoopCost = 0u;
	uint32_t graphStartCostThisFrame = 0u;
	uint64_t graphPlayRequestsThisFrame = 0u;
	uint64_t graphCullsThisFrame = 0u;
	uint64_t graphBudgetRejectsThisFrame = 0u;
	uint64_t graphLodSelectionsThisFrame = 0u;

	uint32_t cueActiveInstances = 0u;
	uint32_t cueActiveTracks = 0u;
	uint64_t cueTrackStartsThisFrame = 0u;
	uint64_t cueBudgetRejectsThisFrame = 0u;
	uint64_t cueBudgetDelaysThisFrame = 0u;
};

struct VfxDiagnosticsSummary
{
	uint32_t sampleCount = 0u;
	float averageFrameMs = 0.0f;
	float maxFrameMs = 0.0f;
	float averageUpdateMs = 0.0f;
	float averageDrawMs = 0.0f;
	float averagePresentMs = 0.0f;
	uint32_t peakEstimatedActiveParticles = 0u;
	uint32_t peakParticleDrawCalls = 0u;
	uint32_t peakGraphActiveLoops = 0u;
	uint32_t peakCueActiveTracks = 0u;
	uint64_t totalGraphCulls = 0u;
	uint64_t totalGraphBudgetRejects = 0u;
	uint64_t totalCueBudgetRejects = 0u;
};

struct VfxGraphStressConfig
{
	std::string graphName = "Phase27ScalableIntegratedExplosion";
	uint32_t oneShotCount = 32u;
	uint32_t loopCount = 8u;
	uint32_t gridColumns = 8u;
	float spacing = 2.0f;
	Vector3 center{ 0.0f, 1.0f, 0.0f };
};

struct VfxGraphStressResult
{
	uint32_t requestedOneShots = 0u;
	uint32_t successfulOneShots = 0u;
	uint32_t requestedLoops = 0u;
	uint32_t successfulLoops = 0u;
	uint32_t stoppedLoops = 0u;
	uint64_t graphBudgetRejects = 0u;
	uint64_t graphCulls = 0u;
	uint32_t estimatedActiveParticlesAfterStart = 0u;
	uint32_t activeEmittersAfterStart = 0u;
	bool graphWasRegistered = false;
};

/// <summary>
/// Phase28 VFX diagnostics collector. Existing CPU-visible counters are sampled without GPU readback or fence waits.
/// </summary>
class VfxGraphDiagnostics
{
public:
	static constexpr std::size_t kHistoryCapacity = 240u;
	static constexpr uint32_t kMaxStressOneShots = 512u;
	static constexpr uint32_t kMaxStressLoops = 128u;

	static VfxGraphDiagnostics* GetInstance()
	{
		static VfxGraphDiagnostics instance;
		return &instance;
	}

	void CaptureFrame()
	{
		const GameTimer::CompletedFrameTiming& timing = GameTimer::GetInstance()->GetCompletedFrameTiming();
		const VfxGraphRuntimeStats& graphStats = VfxGraphRuntime::GetInstance()->GetStats();
		const VfxRuntimeStats& cueStats = VfxCueRuntime::GetInstance()->GetStats();
		GpuParticleManager* particleManager = GpuParticleManager::GetInstance();

		VfxDiagnosticsFrameSample sample{};
		sample.frameNumber = ++frameNumber_;
		sample.frameIntervalMs = timing.frameIntervalMs;
		sample.updateMs = timing.updateMs;
		sample.drawMs = timing.drawMs;
		sample.presentMs = timing.presentMs;
		sample.totalFrameMs = timing.totalFrameMs;
		sample.emitterCount = ToUint32(particleManager->GetEmitterCount());
		sample.activeEmitterCount = ToUint32(particleManager->GetActiveEmitterCount());
		sample.estimatedActiveParticles = particleManager->GetEstimatedActiveParticleCount();
		sample.particleDrawCalls = particleManager->GetLastDrawCallCount();
		sample.graphActiveLoops = graphStats.activeLoopCount;
		sample.graphActiveLoopCost = graphStats.activeLoopCost;
		sample.graphStartCostThisFrame = graphStats.graphStartCostThisFrame;
		sample.cueActiveInstances = cueStats.activeInstanceCount;
		sample.cueActiveTracks = cueStats.activeTrackCount;

		const uint64_t currentEmitDispatches = particleManager->GetEmitDispatchCount();
		if (hasPreviousCounters_)
		{
			sample.emitDispatchesThisFrame = Delta(currentEmitDispatches, previousEmitDispatches_);
			sample.graphPlayRequestsThisFrame = Delta(graphStats.playRequests, previousGraphStats_.playRequests);
			sample.graphCullsThisFrame = Delta(graphStats.culledOneShots + graphStats.loopCullTransitions,
				previousGraphStats_.culledOneShots + previousGraphStats_.loopCullTransitions);
			sample.graphBudgetRejectsThisFrame = Delta(graphStats.budgetRejectedPlays, previousGraphStats_.budgetRejectedPlays);
			sample.graphLodSelectionsThisFrame = Delta(
				graphStats.lodNearSelections + graphStats.lodMidSelections + graphStats.lodFarSelections,
				previousGraphStats_.lodNearSelections + previousGraphStats_.lodMidSelections + previousGraphStats_.lodFarSelections);
			sample.cueTrackStartsThisFrame = Delta(cueStats.totalTrackStarts, previousCueStats_.totalTrackStarts);
			sample.cueBudgetRejectsThisFrame = Delta(cueStats.budgetRejectedInstances, previousCueStats_.budgetRejectedInstances);
			sample.cueBudgetDelaysThisFrame = Delta(cueStats.budgetDelayedTrackStarts, previousCueStats_.budgetDelayedTrackStarts);
		}

		previousEmitDispatches_ = currentEmitDispatches;
		previousGraphStats_ = graphStats;
		previousCueStats_ = cueStats;
		hasPreviousCounters_ = true;

		history_[historyHead_] = sample;
		historyHead_ = (historyHead_ + 1u) % kHistoryCapacity;
		historyCount_ = (std::min)(historyCount_ + 1u, kHistoryCapacity);
	}

	void ResetHistory()
	{
		history_ = {};
		historyHead_ = 0u;
		historyCount_ = 0u;
		frameNumber_ = 0u;
		hasPreviousCounters_ = false;
		previousEmitDispatches_ = 0u;
		previousGraphStats_ = {};
		previousCueStats_ = {};
	}

	[[nodiscard]] std::size_t GetSampleCount() const { return historyCount_; }

	[[nodiscard]] const VfxDiagnosticsFrameSample* GetLatestSample() const
	{
		if (historyCount_ == 0u) return nullptr;
		const std::size_t index = (historyHead_ + kHistoryCapacity - 1u) % kHistoryCapacity;
		return &history_[index];
	}

	[[nodiscard]] const VfxDiagnosticsFrameSample* GetSampleFromOldest(std::size_t index) const
	{
		if (index >= historyCount_) return nullptr;
		const std::size_t oldest = (historyHead_ + kHistoryCapacity - historyCount_) % kHistoryCapacity;
		return &history_[(oldest + index) % kHistoryCapacity];
	}

	[[nodiscard]] VfxDiagnosticsSummary BuildSummary() const
	{
		VfxDiagnosticsSummary summary{};
		summary.sampleCount = static_cast<uint32_t>(historyCount_);
		if (historyCount_ == 0u) return summary;

		for (std::size_t index = 0u; index < historyCount_; ++index)
		{
			const VfxDiagnosticsFrameSample* sample = GetSampleFromOldest(index);
			if (sample == nullptr) continue;
			summary.averageFrameMs += sample->totalFrameMs;
			summary.averageUpdateMs += sample->updateMs;
			summary.averageDrawMs += sample->drawMs;
			summary.averagePresentMs += sample->presentMs;
			summary.maxFrameMs = (std::max)(summary.maxFrameMs, sample->totalFrameMs);
			summary.peakEstimatedActiveParticles = (std::max)(summary.peakEstimatedActiveParticles, sample->estimatedActiveParticles);
			summary.peakParticleDrawCalls = (std::max)(summary.peakParticleDrawCalls, sample->particleDrawCalls);
			summary.peakGraphActiveLoops = (std::max)(summary.peakGraphActiveLoops, sample->graphActiveLoops);
			summary.peakCueActiveTracks = (std::max)(summary.peakCueActiveTracks, sample->cueActiveTracks);
			summary.totalGraphCulls += sample->graphCullsThisFrame;
			summary.totalGraphBudgetRejects += sample->graphBudgetRejectsThisFrame;
			summary.totalCueBudgetRejects += sample->cueBudgetRejectsThisFrame;
		}

		const float divisor = static_cast<float>(historyCount_);
		summary.averageFrameMs /= divisor;
		summary.averageUpdateMs /= divisor;
		summary.averageDrawMs /= divisor;
		summary.averagePresentMs /= divisor;
		return summary;
	}

	bool RunStress(const VfxGraphStressConfig& config)
	{
		StopStressLoops();
		lastStressResult_ = {};
		lastStressResult_.requestedOneShots = (std::min)(config.oneShotCount, kMaxStressOneShots);
		lastStressResult_.requestedLoops = (std::min)(config.loopCount, kMaxStressLoops);

		VfxGraphRuntime* runtime = VfxGraphRuntime::GetInstance();
		lastStressResult_.graphWasRegistered = runtime->IsRegistered(config.graphName);
		if (!lastStressResult_.graphWasRegistered) return false;

		const VfxGraphRuntimeStats before = runtime->GetStats();
		const uint32_t columns = (std::max)(config.gridColumns, 1u);
		for (uint32_t index = 0u; index < lastStressResult_.requestedOneShots; ++index)
		{
			if (runtime->Play(config.graphName, BuildGridPosition(config, index, columns)))
			{
				++lastStressResult_.successfulOneShots;
			}
		}
		for (uint32_t index = 0u; index < lastStressResult_.requestedLoops; ++index)
		{
			VfxGraphPlayHandle handle = runtime->PlayLoop(config.graphName,
				BuildGridPosition(config, lastStressResult_.requestedOneShots + index, columns));
			if (handle.IsValid())
			{
				stressLoopHandles_.push_back(handle);
				++lastStressResult_.successfulLoops;
			}
		}

		const VfxGraphRuntimeStats after = runtime->GetStats();
		lastStressResult_.graphBudgetRejects = Delta(after.budgetRejectedPlays, before.budgetRejectedPlays);
		lastStressResult_.graphCulls = Delta(after.culledOneShots + after.loopCullTransitions,
			before.culledOneShots + before.loopCullTransitions);
		GpuParticleManager* particleManager = GpuParticleManager::GetInstance();
		lastStressResult_.estimatedActiveParticlesAfterStart = particleManager->GetEstimatedActiveParticleCount();
		lastStressResult_.activeEmittersAfterStart = ToUint32(particleManager->GetActiveEmitterCount());
		return lastStressResult_.successfulOneShots > 0u || lastStressResult_.successfulLoops > 0u;
	}

	uint32_t StopStressLoops()
	{
		uint32_t stopped = 0u;
		VfxGraphRuntime* runtime = VfxGraphRuntime::GetInstance();
		for (const VfxGraphPlayHandle& handle : stressLoopHandles_)
		{
			if (runtime->StopLoop(handle)) ++stopped;
		}
		stressLoopHandles_.clear();
		lastStressResult_.stoppedLoops += stopped;
		return stopped;
	}

	[[nodiscard]] const VfxGraphStressResult& GetLastStressResult() const { return lastStressResult_; }
	[[nodiscard]] std::size_t GetActiveStressLoopCount() const { return stressLoopHandles_.size(); }

private:
	VfxGraphDiagnostics() = default;

	static uint64_t Delta(uint64_t current, uint64_t previous)
	{
		return current >= previous ? current - previous : current;
	}

	static uint32_t ToUint32(std::size_t value)
	{
		return static_cast<uint32_t>((std::min)(value, static_cast<std::size_t>((std::numeric_limits<uint32_t>::max)())));
	}

	static Vector3 BuildGridPosition(const VfxGraphStressConfig& config, uint32_t index, uint32_t columns)
	{
		const uint32_t row = index / columns;
		const uint32_t column = index % columns;
		const float halfWidth = static_cast<float>(columns - 1u) * 0.5f;
		return {
			config.center.x + (static_cast<float>(column) - halfWidth) * config.spacing,
			config.center.y,
			config.center.z + static_cast<float>(row) * config.spacing
		};
	}

	std::array<VfxDiagnosticsFrameSample, kHistoryCapacity> history_{};
	std::size_t historyHead_ = 0u;
	std::size_t historyCount_ = 0u;
	uint64_t frameNumber_ = 0u;
	bool hasPreviousCounters_ = false;
	uint64_t previousEmitDispatches_ = 0u;
	VfxGraphRuntimeStats previousGraphStats_{};
	VfxRuntimeStats previousCueStats_{};
	std::vector<VfxGraphPlayHandle> stressLoopHandles_;
	VfxGraphStressResult lastStressResult_{};
};

} // namespace Ken4lowEngine
