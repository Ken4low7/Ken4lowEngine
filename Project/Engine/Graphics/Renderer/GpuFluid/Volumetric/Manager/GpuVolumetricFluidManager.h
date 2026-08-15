#pragma once

#include "../Data/GpuVolumetricFluidEmitterTypes.h"
#include "../Data/GpuVolumetricFluidObstacleTypes.h"
#include "../Data/GpuVolumetricFluidRenderTypes.h"
#include "../Pass/GpuVolumetricFluidEmitterInjectionPass.h"
#include "../Pass/GpuVolumetricFluidForcePass.h"
#include "../Pass/GpuVolumetricFluidObstacleRasterPass.h"
#include "../Pass/GpuVolumetricFluidPressureProjectionPass.h"
#include "../Pass/GpuVolumetricFluidResetPass.h"
#include "../Pass/GpuVolumetricFluidScalarAdvectionPass.h"
#include "../Pass/GpuVolumetricFluidVelocityAdvectionPass.h"
#include "../Renderer/GpuVolumetricFluidRaymarchRenderer.h"
#include "../Resource/GpuVolumetricFluidGridResource.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class ActorWorld;
class ForwardRenderQueue;

enum class GpuVolumetricFluidStressPreset : uint32_t
{
	Off = 0,
	Baseline64,
	Heavy128,
};

struct GpuVolumetricFluidRuntimeStats
{
	uint64_t totalSimulationSteps = 0;
	uint32_t lastFrameSubsteps = 0;
	uint32_t sceneEmitterCount = 0;
	uint32_t sceneObstacleCount = 0;
	uint32_t syntheticEmitterCount = 0;
	uint32_t syntheticObstacleCount = 0;
	uint32_t lastInjectedEmitterCount = 0;
	uint32_t lastCulledEmitterCount = 0;
	uint32_t lastRasterObstacleCount = 0;
	uint32_t lastCulledObstacleCount = 0;
	uint32_t lastPressureIterationCount = 0;
	uint64_t approximateGpuMemoryBytes = 0;
	uint64_t resetCount = 0;
	uint64_t reconfigureCount = 0;
	uint64_t failedReconfigureCount = 0;
	uint64_t duplicateFrameUpdateSkipCount = 0;
	uint64_t velocityDispatchCount = 0;
	uint64_t pressureDispatchCount = 0;
	uint64_t scalarDispatchCount = 0;
	uint64_t forceDispatchCount = 0;
	uint64_t emitterDispatchCount = 0;
	uint64_t obstacleDispatchCount = 0;
	uint64_t forwardDrawCount = 0;
	uint64_t forwardPacketCount = 0;
	float accumulatorSeconds = 0.0f;
	float elapsedSimulationSeconds = 0.0f;
	bool runtimeEnabled = false;
	bool simulationActive = false;
	bool lastStepSucceeded = true;
};

/// Phase17のTexture3D Solver、Scene Source収集、Forward Raymarchを1つのRuntimeへ束ねる所有者。
class GpuVolumetricFluidManager
{
public:
	static constexpr uint32_t kMinGridDimension = 8;
	static constexpr uint32_t kMaxGridDimension = GpuVolumetricFluidGridDesc::kMaxDimension;
	static constexpr float kMinCellSize = 0.001f;
	static constexpr float kMaxCellSize = 10.0f;

	static GpuVolumetricFluidManager* GetInstance();

	bool Initialize();
	void Finalize();

	void UpdateFromWorld(const ActorWorld& world, float deltaTime);
	bool SubmitForward(ForwardRenderQueue& queue);

	void SetRuntimeEnabled(bool enabled);
	[[nodiscard]] bool IsRuntimeEnabled() const { return runtimeEnabled_; }
	void SetPaused(bool paused) { paused_ = paused; }
	[[nodiscard]] bool IsPaused() const { return paused_; }
	void RequestSingleStep() { singleStepRequested_ = true; }
	void RequestReset() { resetRequested_ = true; }

	void SetRenderEnabled(bool enabled) { renderEnabled_ = enabled; }
	[[nodiscard]] bool IsRenderEnabled() const { return renderEnabled_; }

	void RequestGridReconfigure(
		uint32_t width,
		uint32_t height,
		uint32_t depth,
		float cellSize,
		uint32_t pressureIterations);
	void ApplyStressPreset(GpuVolumetricFluidStressPreset preset);
	void SetSyntheticStressCounts(uint32_t emitterCount, uint32_t obstacleCount);

	[[nodiscard]] bool IsInitialized() const { return initialized_; }
	[[nodiscard]] const GpuVolumetricFluidSimulationDesc& GetSimulationDesc() const { return simulationDesc_; }
	[[nodiscard]] GpuVolumetricFluidSimulationDesc& GetEditableSimulationDesc() { return simulationDesc_; }
	[[nodiscard]] const GpuVolumetricFluidDomainMapping& GetDomainMapping() const { return domain_; }
	[[nodiscard]] GpuVolumetricFluidDomainMapping& GetEditableDomainMapping() { return domain_; }
	[[nodiscard]] const GpuVolumetricFluidRenderDesc& GetRenderDesc() const { return renderDesc_; }
	[[nodiscard]] GpuVolumetricFluidRenderDesc& GetEditableRenderDesc() { return renderDesc_; }
	[[nodiscard]] const GpuVolumetricFluidRuntimeStats& GetRuntimeStats() const { return stats_; }
	[[nodiscard]] GpuVolumetricFluidStressPreset GetStressPreset() const { return stressPreset_; }
	[[nodiscard]] uint32_t GetSyntheticEmitterCount() const { return syntheticEmitterCount_; }
	[[nodiscard]] uint32_t GetSyntheticObstacleCount() const { return syntheticObstacleCount_; }

private:
	struct PendingReconfigure
	{
		GpuVolumetricFluidGridDesc grid{};
		uint32_t pressureIterations = 32;
		bool pending = false;
	};

	GpuVolumetricFluidManager();
	~GpuVolumetricFluidManager() = default;
	GpuVolumetricFluidManager(const GpuVolumetricFluidManager&) = delete;
	GpuVolumetricFluidManager& operator=(const GpuVolumetricFluidManager&) = delete;

	bool InitializePasses();
	void ReleaseRuntimeResources();
	bool ApplyPendingReconfigure();
	bool RecreateGrid(const GpuVolumetricFluidGridDesc& gridDesc, uint32_t pressureIterations);
	bool ResetSimulation();
	bool ExecuteSimulationStep(float deltaTime);
	void CollectSceneSources(const ActorWorld& world);
	void AppendSyntheticStressSources();
	void RecenterDomainForGrid(
		const GpuVolumetricFluidGridDesc& oldGrid,
		const GpuVolumetricFluidGridDesc& newGrid);
	void RefreshStats(uint32_t substeps, bool lastStepSucceeded);

private:
	GpuVolumetricFluidSimulationDesc simulationDesc_{};
	GpuVolumetricFluidDomainMapping domain_{};
	GpuVolumetricFluidRenderDesc renderDesc_{};
	GpuVolumetricFluidGridResource grid_{};
	GpuVolumetricFluidVelocityAdvectionPass velocityAdvectionPass_{};
	GpuVolumetricFluidPressureProjectionPass pressureProjectionPass_{};
	GpuVolumetricFluidScalarAdvectionPass scalarAdvectionPass_{};
	GpuVolumetricFluidForcePass forcePass_{};
	GpuVolumetricFluidEmitterInjectionPass emitterInjectionPass_{};
	GpuVolumetricFluidObstacleRasterPass obstacleRasterPass_{};
	GpuVolumetricFluidResetPass resetPass_{};
	GpuVolumetricFluidRaymarchRenderer forwardRenderer_{};
	std::vector<GpuVolumetricFluidEmitterSource> emitters_{};
	std::vector<GpuVolumetricFluidObstacleSource> obstacles_{};
	PendingReconfigure pendingReconfigure_{};
	GpuVolumetricFluidRuntimeStats stats_{};
	GpuVolumetricFluidStressPreset stressPreset_ = GpuVolumetricFluidStressPreset::Off;
	const ActorWorld* activeWorld_ = nullptr;
	uint32_t syntheticEmitterCount_ = 0;
	uint32_t syntheticObstacleCount_ = 0;
	uint32_t sceneEmitterCount_ = 0;
	uint32_t sceneObstacleCount_ = 0;
	uint32_t lastUpdateFrameIndex_ = UINT32_MAX;
	uint64_t lastUpdateFrameFenceValue_ = UINT64_MAX;
	float accumulatorSeconds_ = 0.0f;
	float elapsedSimulationSeconds_ = 0.0f;
	bool initialized_ = false;
	bool runtimeEnabled_ = false; // Phase16 Sceneを変えないため、3D SolverはEditor/Gameplayから明示的に有効化する。
	bool paused_ = false;
	bool singleStepRequested_ = false;
	bool resetRequested_ = false;
	bool simulationActive_ = false;
	bool renderEnabled_ = true;
};

} // namespace Ken4lowEngine
