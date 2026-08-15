#include "GpuVolumetricFluidManager.h"

#include "../Renderer/GpuVolumetricFluidForwardRenderBridge.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"
#include "Engine/Scene/Actor/Components/FluidEmitterComponent.h"
#include "Engine/Scene/Actor/Components/GpuVolumetricFluidColliderObstacleAdapter.h"
#include "Engine/Scene/Actor/Core/Actor.h"
#include "Engine/Scene/Actor/Core/ActorWorld.h"
#include <DirectXCommon.h>
#include <DX12FenceManager.h>

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{

GpuVolumetricFluidManager* GpuVolumetricFluidManager::GetInstance()
{
	static GpuVolumetricFluidManager instance;
	return &instance;
}

GpuVolumetricFluidManager::GpuVolumetricFluidManager()
{
	const float halfWidth = static_cast<float>(simulationDesc_.grid.width) * simulationDesc_.grid.cellSize * 0.5f;
	const float halfHeight = static_cast<float>(simulationDesc_.grid.height) * simulationDesc_.grid.cellSize * 0.5f;
	const float halfDepth = static_cast<float>(simulationDesc_.grid.depth) * simulationDesc_.grid.cellSize * 0.5f;
	domain_.origin = { -halfWidth, -halfHeight, -halfDepth };
}

bool GpuVolumetricFluidManager::Initialize()
{
	if (initialized_)
	{
		return true;
	}
	if (!simulationDesc_.IsValid() || !domain_.IsValid())
	{
		return false;
	}

	ReleaseRuntimeResources();
	if (!grid_.Initialize(simulationDesc_.grid) || !InitializePasses())
	{
		ReleaseRuntimeResources();
		return false;
	}

	initialized_ = true;
	if (!ResetSimulation())
	{
		ReleaseRuntimeResources();
		return false;
	}

	resetRequested_ = false; // 初回Allocate直後は既に全fieldをReset済みなので、同じFrameで二重Clearしない。
	RefreshStats(0, true);
	return true;
}

void GpuVolumetricFluidManager::Finalize()
{
	ReleaseRuntimeResources();

	simulationDesc_ = {};
	domain_ = {};
	const float halfWidth = static_cast<float>(simulationDesc_.grid.width) * simulationDesc_.grid.cellSize * 0.5f;
	const float halfHeight = static_cast<float>(simulationDesc_.grid.height) * simulationDesc_.grid.cellSize * 0.5f;
	const float halfDepth = static_cast<float>(simulationDesc_.grid.depth) * simulationDesc_.grid.cellSize * 0.5f;
	domain_.origin = { -halfWidth, -halfHeight, -halfDepth };
	renderDesc_ = {};
	pendingReconfigure_ = {};
	stats_ = {};
	stressPreset_ = GpuVolumetricFluidStressPreset::Off;
	activeWorld_ = nullptr;
	syntheticEmitterCount_ = 0;
	syntheticObstacleCount_ = 0;
	sceneEmitterCount_ = 0;
	sceneObstacleCount_ = 0;
	lastUpdateFrameIndex_ = UINT32_MAX;
	lastUpdateFrameFenceValue_ = UINT64_MAX;
	accumulatorSeconds_ = 0.0f;
	elapsedSimulationSeconds_ = 0.0f;
	runtimeEnabled_ = false;
	paused_ = false;
	singleStepRequested_ = false;
	resetRequested_ = false;
	simulationActive_ = false;
	renderEnabled_ = true;
}

bool GpuVolumetricFluidManager::InitializePasses()
{
	return velocityAdvectionPass_.Initialize() &&
		pressureProjectionPass_.Initialize() &&
		scalarAdvectionPass_.Initialize() &&
		forcePass_.Initialize() &&
		emitterInjectionPass_.Initialize() &&
		obstacleRasterPass_.Initialize() &&
		resetPass_.Initialize() &&
		forwardRenderer_.Initialize();
}

void GpuVolumetricFluidManager::ReleaseRuntimeResources()
{
	forwardRenderer_.Finalize();
	resetPass_.Finalize();
	obstacleRasterPass_.Finalize();
	emitterInjectionPass_.Finalize();
	forcePass_.Finalize();
	scalarAdvectionPass_.Finalize();
	pressureProjectionPass_.Finalize();
	velocityAdvectionPass_.Finalize();
	grid_.Finalize();
	emitters_.clear();
	obstacles_.clear();
	initialized_ = false;
	simulationActive_ = false;
}

void GpuVolumetricFluidManager::SetRuntimeEnabled(bool enabled)
{
	if (runtimeEnabled_ == enabled)
	{
		return;
	}

	runtimeEnabled_ = enabled;
	if (enabled)
	{
		// 既存Resourceを再利用する再Enable時だけResetを予約する。初回Initializeは自身でResetする。
		resetRequested_ = initialized_;
	}
	else
	{
		simulationActive_ = false;
		accumulatorSeconds_ = 0.0f;
		singleStepRequested_ = false;
	}
}

void GpuVolumetricFluidManager::UpdateFromWorld(const ActorWorld& world, float deltaTime)
{
	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	DX12CommandManager* commandManager = dxCommon ? dxCommon->GetCommandManager() : nullptr;
	if (commandManager == nullptr)
	{
		RefreshStats(0, false);
		return;
	}

	const uint32_t frameIndex = commandManager->GetCurrentFrameIndex();
	const uint64_t frameFenceValue = commandManager->GetFrameFenceValue(frameIndex);
	const bool worldChanged = activeWorld_ != &world;
	if (!worldChanged &&
		lastUpdateFrameIndex_ == frameIndex &&
		lastUpdateFrameFenceValue_ == frameFenceValue)
	{
		++stats_.duplicateFrameUpdateSkipCount;
		return; // Reflection/Probeが同一Worldを再描画しても3D Solverは1 Engine Frameに1回だけ進める。
	}
	lastUpdateFrameIndex_ = frameIndex;
	lastUpdateFrameFenceValue_ = frameFenceValue;
	activeWorld_ = &world;

	if (!runtimeEnabled_)
	{
		// OFF中もEditor診断用のScene source数だけは更新し、Texture3D ResourceはAllocateしない。
		CollectSceneSources(world);
		simulationActive_ = false;
		RefreshStats(0, true);
		return;
	}

	bool initializedThisFrame = false;
	if (!initialized_)
	{
		if (!Initialize())
		{
			RefreshStats(0, false);
			return;
		}
		initializedThisFrame = true;
	}

	if (!ApplyPendingReconfigure())
	{
		RefreshStats(0, false);
		return;
	}

	// Initialize/Reconfigure後の最新Domain/GridでSourceを構築し、初回FrameからEmitterを反映する。
	CollectSceneSources(world);
	AppendSyntheticStressSources();

	if ((worldChanged && !initializedThisFrame) || resetRequested_)
	{
		resetRequested_ = false;
		if (!ResetSimulation())
		{
			RefreshStats(0, false);
			return;
		}
	}

	if (!emitters_.empty())
	{
		simulationActive_ = true; // Source停止後も既存Densityが散逸するまでSimulationを継続する。
	}

	uint32_t substeps = 0;
	bool lastStepSucceeded = true;
	const float fixedDeltaTime = simulationDesc_.fixedDeltaTime;
	if (singleStepRequested_)
	{
		singleStepRequested_ = false;
		lastStepSucceeded = ExecuteSimulationStep(fixedDeltaTime);
		substeps = lastStepSucceeded ? 1u : 0u;
	}
	else if (!paused_ && simulationActive_)
	{
		const float maxAccumulation = fixedDeltaTime * static_cast<float>(simulationDesc_.maxSubsteps);
		accumulatorSeconds_ += std::clamp(deltaTime, 0.0f, maxAccumulation);
		while (accumulatorSeconds_ >= fixedDeltaTime && substeps < simulationDesc_.maxSubsteps)
		{
			if (!ExecuteSimulationStep(fixedDeltaTime))
			{
				lastStepSucceeded = false;
				break;
			}
			accumulatorSeconds_ -= fixedDeltaTime;
			++substeps;
		}
	}

	RefreshStats(substeps, lastStepSucceeded);
}

bool GpuVolumetricFluidManager::SubmitForward(ForwardRenderQueue& queue)
{
	if (!runtimeEnabled_ || !initialized_ || !renderEnabled_ || !simulationActive_)
	{
		return false;
	}

	const bool submitted = GpuVolumetricFluidForwardRenderBridge::GetInstance()->Submit(
		queue,
		forwardRenderer_,
		grid_,
		domain_,
		renderDesc_);
	RefreshStats(stats_.lastFrameSubsteps, stats_.lastStepSucceeded);
	return submitted;
}

void GpuVolumetricFluidManager::RequestGridReconfigure(
	uint32_t width,
	uint32_t height,
	uint32_t depth,
	float cellSize,
	uint32_t pressureIterations)
{
	GpuVolumetricFluidGridDesc gridDesc{};
	gridDesc.width = width;
	gridDesc.height = height;
	gridDesc.depth = depth;
	gridDesc.cellSize = cellSize;
	if (!gridDesc.IsValid() ||
		pressureIterations == 0 ||
		pressureIterations > GpuVolumetricFluidSimulationDesc::kMaxPressureIterations)
	{
		return;
	}

	if (!initialized_)
	{
		const GpuVolumetricFluidGridDesc oldGrid = simulationDesc_.grid;
		RecenterDomainForGrid(oldGrid, gridDesc);
		simulationDesc_.grid = gridDesc;
		simulationDesc_.pressureIterations = pressureIterations;
		pendingReconfigure_ = {};
		return; // 未生成ならGPU Resourceを一度作って捨てず、最初のInitializeへ設定だけ渡す。
	}

	pendingReconfigure_.grid = gridDesc;
	pendingReconfigure_.pressureIterations = pressureIterations;
	pendingReconfigure_.pending = true;
}

void GpuVolumetricFluidManager::ApplyStressPreset(GpuVolumetricFluidStressPreset preset)
{
	stressPreset_ = preset;
	switch (preset)
	{
	case GpuVolumetricFluidStressPreset::Baseline64:
		SetRuntimeEnabled(true);
		syntheticEmitterCount_ = 8;
		syntheticObstacleCount_ = 8;
		renderDesc_.maxSteps = 192;
		RequestGridReconfigure(64, 64, 64, 0.25f, 32);
		break;
	case GpuVolumetricFluidStressPreset::Heavy128:
		SetRuntimeEnabled(true);
		syntheticEmitterCount_ = 24;
		syntheticObstacleCount_ = 24;
		renderDesc_.maxSteps = 256;
		RequestGridReconfigure(128, 128, 128, 0.125f, 48);
		break;
	case GpuVolumetricFluidStressPreset::Off:
	default:
		syntheticEmitterCount_ = 0;
		syntheticObstacleCount_ = 0;
		break;
	}

	if (syntheticEmitterCount_ > 0)
	{
		simulationActive_ = true;
	}
}

void GpuVolumetricFluidManager::SetSyntheticStressCounts(uint32_t emitterCount, uint32_t obstacleCount)
{
	syntheticEmitterCount_ = (std::min)(emitterCount, 256u);
	syntheticObstacleCount_ = (std::min)(obstacleCount, 256u);
	stressPreset_ = GpuVolumetricFluidStressPreset::Off;
	if (syntheticEmitterCount_ > 0)
	{
		SetRuntimeEnabled(true);
		simulationActive_ = true;
	}
}

bool GpuVolumetricFluidManager::ApplyPendingReconfigure()
{
	if (!pendingReconfigure_.pending)
	{
		return true;
	}

	const PendingReconfigure request = pendingReconfigure_;
	pendingReconfigure_.pending = false;
	const bool succeeded = RecreateGrid(request.grid, request.pressureIterations);
	if (succeeded)
	{
		resetRequested_ = false; // RecreateGrid内でReset済みなので、同じFrameの再Clearを抑止する。
	}
	return succeeded;
}

bool GpuVolumetricFluidManager::RecreateGrid(
	const GpuVolumetricFluidGridDesc& gridDesc,
	uint32_t pressureIterations)
{
	if (!gridDesc.IsValid() ||
		pressureIterations == 0 ||
		pressureIterations > GpuVolumetricFluidSimulationDesc::kMaxPressureIterations)
	{
		++stats_.failedReconfigureCount;
		return false;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetFenceManager() == nullptr)
	{
		++stats_.failedReconfigureCount;
		return false;
	}

	DX12FenceManager* fence = dxCommon->GetFenceManager();
	fence->WaitForValue(fence->GetCurrentValue()); // Resolution変更時だけGPU参照を安全化し、通常Frameでは待機を増やさない。

	const GpuVolumetricFluidGridDesc oldGrid = simulationDesc_.grid;
	const uint32_t oldPressureIterations = simulationDesc_.pressureIterations;
	const Vector3 oldDomainOrigin = domain_.origin;

	grid_.Finalize();
	RecenterDomainForGrid(oldGrid, gridDesc);
	simulationDesc_.grid = gridDesc;
	simulationDesc_.pressureIterations = pressureIterations;

	if (!grid_.Initialize(gridDesc) || !ResetSimulation())
	{
		grid_.Finalize();
		simulationDesc_.grid = oldGrid;
		simulationDesc_.pressureIterations = oldPressureIterations;
		domain_.origin = oldDomainOrigin;
		++stats_.failedReconfigureCount;

		// 旧Gridの復旧成否に関係なく、要求されたReconfigure自体は失敗として呼び出し側へ返す。
		if (grid_.Initialize(oldGrid))
		{
			ResetSimulation();
		}
		return false;
	}

	++stats_.reconfigureCount;
	return true;
}

void GpuVolumetricFluidManager::RecenterDomainForGrid(
	const GpuVolumetricFluidGridDesc& oldGrid,
	const GpuVolumetricFluidGridDesc& newGrid)
{
	const Vector3 axisU = Vector3::NormalizeSafe(domain_.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain_.axisV, { 0.0f, 1.0f, 0.0f });
	const Vector3 axisW = Vector3::NormalizeSafe(domain_.axisW, { 0.0f, 0.0f, 1.0f });
	const Vector3 center = domain_.origin +
		axisU * (static_cast<float>(oldGrid.width) * oldGrid.cellSize * 0.5f) +
		axisV * (static_cast<float>(oldGrid.height) * oldGrid.cellSize * 0.5f) +
		axisW * (static_cast<float>(oldGrid.depth) * oldGrid.cellSize * 0.5f);

	domain_.origin = center -
		axisU * (static_cast<float>(newGrid.width) * newGrid.cellSize * 0.5f) -
		axisV * (static_cast<float>(newGrid.height) * newGrid.cellSize * 0.5f) -
		axisW * (static_cast<float>(newGrid.depth) * newGrid.cellSize * 0.5f); // Stress変更でもWorld-space Volume中心を固定する。
}

bool GpuVolumetricFluidManager::ResetSimulation()
{
	if (!initialized_ || !grid_.IsInitialized() || !resetPass_.Reset(grid_))
	{
		return false;
	}

	accumulatorSeconds_ = 0.0f;
	elapsedSimulationSeconds_ = 0.0f;
	simulationActive_ = runtimeEnabled_ && syntheticEmitterCount_ > 0;
	return true;
}

bool GpuVolumetricFluidManager::ExecuteSimulationStep(float deltaTime)
{
	if (!obstacleRasterPass_.Dispatch(
		grid_, simulationDesc_, domain_, obstacles_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}
	if (!emitterInjectionPass_.Dispatch(
		grid_, simulationDesc_, domain_, emitters_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}
	if (!velocityAdvectionPass_.Dispatch(
		grid_, simulationDesc_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}
	if (!pressureProjectionPass_.Dispatch(
		grid_, simulationDesc_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}
	if (!scalarAdvectionPass_.DispatchAll(
		grid_, simulationDesc_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}
	if (!forcePass_.DispatchAll(
		grid_, simulationDesc_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}
	if (!pressureProjectionPass_.Dispatch(
		grid_, simulationDesc_, deltaTime, elapsedSimulationSeconds_))
	{
		return false;
	}

	// Phase17の固定Step順をManagerだけに置き、Editor/GameplayからPass順序を崩せないようにする。
	elapsedSimulationSeconds_ += deltaTime;
	++stats_.totalSimulationSteps;
	return true;
}

void GpuVolumetricFluidManager::CollectSceneSources(const ActorWorld& world)
{
	emitters_.clear();
	for (const auto& actor : world.GetActors())
	{
		if (!actor || actor->IsPendingDestroy() || !actor->IsActive())
		{
			continue;
		}

		for (const auto& component : actor->GetComponents())
		{
			const auto* emitter = dynamic_cast<const FluidEmitterComponent*>(component.get());
			if (emitter == nullptr)
			{
				continue;
			}

			const GpuVolumetricFluidEmitterSource source = emitter->BuildVolumetricEmitterSource();
			if (source.enabled && source.IsValid())
			{
				emitters_.push_back(source);
			}
		}
	}

	GpuVolumetricFluidColliderObstacleAdapter::CollectSources(world, obstacles_);
	sceneEmitterCount_ = static_cast<uint32_t>(emitters_.size());
	sceneObstacleCount_ = static_cast<uint32_t>(obstacles_.size());
}

void GpuVolumetricFluidManager::AppendSyntheticStressSources()
{
	const GpuVolumetricFluidGridDesc& gridDesc = simulationDesc_.grid;
	const Vector3 axisU = Vector3::NormalizeSafe(domain_.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain_.axisV, { 0.0f, 1.0f, 0.0f });
	const Vector3 axisW = Vector3::NormalizeSafe(domain_.axisW, { 0.0f, 0.0f, 1.0f });
	const float widthWorld = static_cast<float>(gridDesc.width) * gridDesc.cellSize;
	const float heightWorld = static_cast<float>(gridDesc.height) * gridDesc.cellSize;
	const float depthWorld = static_cast<float>(gridDesc.depth) * gridDesc.cellSize;
	const float minExtent = (std::min)({ widthWorld, heightWorld, depthWorld });

	if (syntheticEmitterCount_ > 0)
	{
		const uint32_t lattice = (std::max)(
			1u,
			static_cast<uint32_t>(std::ceil(std::cbrt(static_cast<float>(syntheticEmitterCount_)))));
		for (uint32_t index = 0; index < syntheticEmitterCount_; ++index)
		{
			const uint32_t x = index % lattice;
			const uint32_t z = (index / lattice) % lattice;
			const uint32_t y = index / (lattice * lattice);
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(lattice);
			const float v = 0.08f + 0.08f * static_cast<float>(y % 3u);
			const float w = (static_cast<float>(z) + 0.5f) / static_cast<float>(lattice);

			GpuVolumetricFluidEmitterSource source{};
			source.worldPosition = domain_.origin +
				axisU * (widthWorld * u) +
				axisV * (heightWorld * v) +
				axisW * (depthWorld * w);
			source.worldVelocity = axisV * 2.5f;
			source.radius = (std::max)(gridDesc.cellSize * 2.0f, minExtent * 0.025f);
			source.velocityStrength = 1.5f;
			source.densityRate = 4.0f;
			source.temperatureRate = 3.0f;
			source.falloffExponent = 2.0f;
			emitters_.push_back(source);
		}
	}

	if (syntheticObstacleCount_ > 0)
	{
		const uint32_t lattice = (std::max)(
			1u,
			static_cast<uint32_t>(std::ceil(std::cbrt(static_cast<float>(syntheticObstacleCount_)))));
		for (uint32_t index = 0; index < syntheticObstacleCount_; ++index)
		{
			const uint32_t x = index % lattice;
			const uint32_t z = (index / lattice) % lattice;
			const uint32_t y = index / (lattice * lattice);
			const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(lattice);
			const float v = 0.35f + 0.15f * static_cast<float>(y % 2u);
			const float w = (static_cast<float>(z) + 0.5f) / static_cast<float>(lattice);

			GpuVolumetricFluidObstacleSource obstacle{};
			obstacle.shape = GpuVolumetricFluidObstacleShape::Sphere;
			obstacle.worldCenter = domain_.origin +
				axisU * (widthWorld * u) +
				axisV * (heightWorld * v) +
				axisW * (depthWorld * w);
			obstacle.radius = (std::max)(gridDesc.cellSize * 1.5f, minExtent * 0.018f);
			obstacles_.push_back(obstacle);
		}
	}
}

void GpuVolumetricFluidManager::RefreshStats(uint32_t substeps, bool lastStepSucceeded)
{
	stats_.lastFrameSubsteps = substeps;
	stats_.sceneEmitterCount = sceneEmitterCount_;
	stats_.sceneObstacleCount = sceneObstacleCount_;
	stats_.syntheticEmitterCount = syntheticEmitterCount_;
	stats_.syntheticObstacleCount = syntheticObstacleCount_;
	stats_.lastInjectedEmitterCount = emitterInjectionPass_.GetLastInjectedSourceCount();
	stats_.lastCulledEmitterCount = emitterInjectionPass_.GetLastCulledSourceCount();
	stats_.lastRasterObstacleCount = obstacleRasterPass_.GetLastObstacleCount();
	stats_.lastCulledObstacleCount = obstacleRasterPass_.GetLastCulledObstacleCount();
	stats_.lastPressureIterationCount = pressureProjectionPass_.GetLastPressureIterationCount();
	stats_.approximateGpuMemoryBytes = grid_.IsInitialized() ? grid_.GetApproximateGpuMemoryBytes() : 0;
	stats_.resetCount = resetPass_.GetResetCount();
	stats_.velocityDispatchCount = velocityAdvectionPass_.GetDispatchCount();
	stats_.pressureDispatchCount = pressureProjectionPass_.GetDispatchCount();
	stats_.scalarDispatchCount = scalarAdvectionPass_.GetDispatchCount();
	stats_.forceDispatchCount = forcePass_.GetDispatchCount();
	stats_.emitterDispatchCount = emitterInjectionPass_.GetDispatchCount();
	stats_.obstacleDispatchCount = obstacleRasterPass_.GetDispatchCount();
	stats_.forwardDrawCount = forwardRenderer_.GetDrawCount();
	stats_.forwardPacketCount = static_cast<uint64_t>(
		GpuVolumetricFluidForwardRenderBridge::GetInstance()->GetLastPacketStats().submittedPackets);
	stats_.accumulatorSeconds = accumulatorSeconds_;
	stats_.elapsedSimulationSeconds = elapsedSimulationSeconds_;
	stats_.runtimeEnabled = runtimeEnabled_;
	stats_.simulationActive = simulationActive_;
	stats_.lastStepSucceeded = lastStepSucceeded;
}

} // namespace Ken4lowEngine
