#pragma once

#include "../Data/GpuFluidEmitterTypes.h"
#include "../Data/GpuFluidObstacleTypes.h"
#include "../Data/GpuFluidRenderTypes.h"
#include "../Pass/GpuFluidEmitterInjectionPass.h"
#include "../Pass/GpuFluidForcePass.h"
#include "../Pass/GpuFluidObstacleRasterPass.h"
#include "../Pass/GpuFluidPressureProjectionPass.h"
#include "../Pass/GpuFluidResetPass.h"
#include "../Pass/GpuFluidScalarAdvectionPass.h"
#include "../Pass/GpuFluidVelocityAdvectionPass.h"
#include "../Renderer/GpuFluidForwardRenderer.h"
#include "../Resource/GpuFluidGridResource.h"

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

class ActorWorld;
class ForwardRenderQueue;

enum class GpuFluidStressPreset : uint32_t
{
	Off = 0,
	Medium,
	Heavy,
	Extreme,
};

struct GpuFluidRuntimeStats
{
	uint64_t totalSimulationSteps = 0;
	uint32_t lastFrameSubsteps = 0;
	uint32_t sceneEmitterCount = 0;
	uint32_t sceneObstacleCount = 0;
	uint32_t syntheticEmitterCount = 0;
	uint32_t syntheticObstacleCount = 0;
	uint64_t approximateGpuMemoryBytes = 0;
	uint64_t resetCount = 0;
	uint64_t velocityDispatchCount = 0;
	uint64_t pressureDispatchCount = 0;
	uint64_t scalarDispatchCount = 0;
	uint64_t forceDispatchCount = 0;
	uint64_t emitterDispatchCount = 0;
	uint64_t obstacleDispatchCount = 0;
	uint64_t forwardDrawCount = 0;
	float accumulatorSeconds = 0.0f;
	float elapsedSimulationSeconds = 0.0f;
	bool simulationActive = false;
	bool lastStepSucceeded = true;
};

/// Phase16の各GPU Fluid Passを固定Step順に束ねるRuntime所有者。
class GpuFluidManager
{
public:
	static constexpr uint32_t kMinGridDimension = 8;
	static constexpr uint32_t kMaxGridDimension = 2048;
	static constexpr uint32_t kMaxPressureIterations = 256;
	static constexpr float kMinCellSize = 0.001f;
	static constexpr float kMaxCellSize = 10.0f;

	static GpuFluidManager* GetInstance();

	bool Initialize();
	void Finalize();

	void UpdateFromWorld(const ActorWorld& world, float deltaTime);
	bool SubmitForward(ForwardRenderQueue& queue);

	void SetPaused(bool paused) { paused_ = paused; }
	[[nodiscard]] bool IsPaused() const { return paused_; }
	void RequestSingleStep() { singleStepRequested_ = true; }
	void RequestReset() { resetRequested_ = true; }

	void SetRenderEnabled(bool enabled) { renderEnabled_ = enabled; }
	[[nodiscard]] bool IsRenderEnabled() const { return renderEnabled_; }

	void RequestGridReconfigure(uint32_t width, uint32_t height, float cellSize, uint32_t pressureIterations);
	void ApplyStressPreset(GpuFluidStressPreset preset);
	void SetSyntheticStressCounts(uint32_t emitterCount, uint32_t obstacleCount);

	[[nodiscard]] bool IsInitialized() const { return initialized_; }
	[[nodiscard]] const GpuFluidSimulationDesc& GetSimulationDesc() const { return simulationDesc_; }
	[[nodiscard]] GpuFluidSimulationDesc& GetEditableSimulationDesc() { return simulationDesc_; }
	[[nodiscard]] const GpuFluidDomainMapping& GetDomainMapping() const { return domain_; }
	[[nodiscard]] GpuFluidDomainMapping& GetEditableDomainMapping() { return domain_; }
	[[nodiscard]] const GpuFluidRenderDesc& GetRenderDesc() const { return renderDesc_; }
	[[nodiscard]] GpuFluidRenderDesc& GetEditableRenderDesc() { return renderDesc_; }
	[[nodiscard]] const GpuFluidRuntimeStats& GetRuntimeStats() const { return stats_; }
	[[nodiscard]] GpuFluidStressPreset GetStressPreset() const { return stressPreset_; }
	[[nodiscard]] uint32_t GetSyntheticEmitterCount() const { return syntheticEmitterCount_; }
	[[nodiscard]] uint32_t GetSyntheticObstacleCount() const { return syntheticObstacleCount_; }
	[[nodiscard]] uint64_t GetForwardDrawCount() const { return forwardRenderer_.GetDrawCount(); }

private:
	struct PendingReconfigure
	{
		GpuFluidGridDesc grid{};
		uint32_t pressureIterations = 40;
		bool pending = false;
	};

	GpuFluidManager() = default;
	~GpuFluidManager() = default;
	GpuFluidManager(const GpuFluidManager&) = delete;
	GpuFluidManager& operator=(const GpuFluidManager&) = delete;

	bool InitializePasses();
	bool ApplyPendingReconfigure();
	bool RecreateGrid(const GpuFluidGridDesc& gridDesc, uint32_t pressureIterations);
	bool ResetSimulation();
	bool ExecuteSimulationStep(float deltaTime);
	void CollectSceneSources(const ActorWorld& world);
	void AppendSyntheticStressSources();
	void RefreshStats(uint32_t substeps, bool lastStepSucceeded);

private:
	GpuFluidSimulationDesc simulationDesc_{};
	GpuFluidDomainMapping domain_{};
	GpuFluidRenderDesc renderDesc_{};
	GpuFluidGridResource grid_{};
	GpuFluidVelocityAdvectionPass velocityAdvectionPass_{};
	GpuFluidPressureProjectionPass pressureProjectionPass_{};
	GpuFluidScalarAdvectionPass scalarAdvectionPass_{};
	GpuFluidForcePass forcePass_{};
	GpuFluidEmitterInjectionPass emitterInjectionPass_{};
	GpuFluidObstacleRasterPass obstacleRasterPass_{};
	GpuFluidResetPass resetPass_{};
	GpuFluidForwardRenderer forwardRenderer_{};
	std::vector<GpuFluidEmitterSource> emitters_{};
	std::vector<GpuFluidObstacleSource> obstacles_{};
	PendingReconfigure pendingReconfigure_{};
	GpuFluidRuntimeStats stats_{};
	GpuFluidStressPreset stressPreset_ = GpuFluidStressPreset::Off;
	const ActorWorld* activeWorld_ = nullptr;
	uint32_t syntheticEmitterCount_ = 0;
	uint32_t syntheticObstacleCount_ = 0;
	uint32_t lastUpdateFrameIndex_ = UINT32_MAX;
	uint64_t lastUpdateFrameFenceValue_ = UINT64_MAX;
	float accumulatorSeconds_ = 0.0f;
	float elapsedSimulationSeconds_ = 0.0f;
	bool initialized_ = false;
	bool paused_ = false;
	bool singleStepRequested_ = false;
	bool resetRequested_ = false;
	bool simulationActive_ = false;
	bool renderEnabled_ = true;
};

} // namespace Ken4lowEngine
