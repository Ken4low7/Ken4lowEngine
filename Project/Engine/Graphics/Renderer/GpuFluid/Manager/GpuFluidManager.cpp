#include "GpuFluidManager.h"

#include "../Renderer/GpuFluidForwardRenderBridge.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"
#include "Engine/Scene/Actor/Components/FluidEmitterComponent.h"
#include "Engine/Scene/Actor/Components/GpuFluidColliderObstacleAdapter.h"
#include "Engine/Scene/Actor/Core/Actor.h"
#include "Engine/Scene/Actor/Core/ActorWorld.h"
#include <DirectXCommon.h>
#include <DX12FenceManager.h>

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{

GpuFluidManager* GpuFluidManager::GetInstance()
{
	static GpuFluidManager instance;
	return &instance;
}

bool GpuFluidManager::Initialize()
{
	Finalize();

	if (!simulationDesc_.IsValid())
	{
		return false;
	}

	const float halfWidth = static_cast<float>(simulationDesc_.grid.width) * simulationDesc_.grid.cellSize * 0.5f;
	const float halfHeight = static_cast<float>(simulationDesc_.grid.height) * simulationDesc_.grid.cellSize * 0.5f;
	domain_.origin = { -halfWidth, -halfHeight, 0.0f };
	domain_.axisU = { 1.0f, 0.0f, 0.0f };
	domain_.axisV = { 0.0f, 1.0f, 0.0f };

	if (!grid_.Initialize(simulationDesc_.grid) || !InitializePasses())
	{
		Finalize();
		return false;
	}

	initialized_ = true;
	if (!ResetSimulation())
	{
		Finalize();
		return false;
	}
	RefreshStats(0, true);
	return true;
}

void GpuFluidManager::Finalize()
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
	pendingReconfigure_ = {};
	stats_ = {};
	stressPreset_ = GpuFluidStressPreset::Off;
	activeWorld_ = nullptr;
	syntheticEmitterCount_ = 0;
	syntheticObstacleCount_ = 0;
	lastUpdateFrameIndex_ = UINT32_MAX;
	lastUpdateFrameFenceValue_ = UINT64_MAX;
	accumulatorSeconds_ = 0.0f;
	elapsedSimulationSeconds_ = 0.0f;
	initialized_ = false;
	paused_ = false;
	singleStepRequested_ = false;
	resetRequested_ = false;
	simulationActive_ = false;
	renderEnabled_ = true;
}

bool GpuFluidManager::InitializePasses()
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

void GpuFluidManager::UpdateFromWorld(const ActorWorld& world, float deltaTime)
{
	if (!initialized_ && !Initialize())
	{
		return; // ActorWorld側に初期化順依存を増やさず、最初にFluidが描画される時点でもRuntimeを起動できるようにする。
	}

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
		return; // Reflection Probe等が同一Engine Frameで同じWorldを再描画してもSimulationは1回だけ進める。
	}
	lastUpdateFrameIndex_ = frameIndex;
	lastUpdateFrameFenceValue_ = frameFenceValue;

	if (!ApplyPendingReconfigure())
	{
		RefreshStats(0, false);
		return;
	}

	if (worldChanged)
	{
		activeWorld_ = &world;
		if (!ResetSimulation())
		{
			RefreshStats(0, false);
			return;
		}
	}

	if (resetRequested_)
	{
		resetRequested_ = false;
		if (!ResetSimulation())
		{
			RefreshStats(0, false);
			return;
		}
	}

	CollectSceneSources(world);
	AppendSyntheticStressSources();

	if (!emitters_.empty())
	{
		simulationActive_ = true; // 一度Densityが入った後もEmitter停止後の減衰を継続できるようActive状態を保持する。
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

bool GpuFluidManager::SubmitForward(ForwardRenderQueue& queue)
{
	if (!initialized_ || !renderEnabled_ || !simulationActive_)
	{
		return false;
	}

	const bool submitted = GpuFluidForwardRenderBridge::GetInstance()->Submit(
		queue,
		forwardRenderer_,
		grid_,
		domain_,
		renderDesc_);
	RefreshStats(stats_.lastFrameSubsteps, stats_.lastStepSucceeded);
	return submitted;
}

void GpuFluidManager::RequestGridReconfigure(
	uint32_t width,
	uint32_t height,
	float cellSize,
	uint32_t pressureIterations)
{
	GpuFluidGridDesc gridDesc{};
	gridDesc.width = width;
	gridDesc.height = height;
	gridDesc.cellSize = cellSize;
	if (!gridDesc.IsValid() || pressureIterations == 0)
	{
		return;
	}

	pendingReconfigure_.grid = gridDesc;
	pendingReconfigure_.pressureIterations = pressureIterations;
	pendingReconfigure_.pending = true; // Editor Draw中のResource破棄を避け、次Update冒頭でGPU安全化して適用する。
}

void GpuFluidManager::ApplyStressPreset(GpuFluidStressPreset preset)
{
	stressPreset_ = preset;
	switch (preset)
	{
	case GpuFluidStressPreset::Medium:
		syntheticEmitterCount_ = 8;
		syntheticObstacleCount_ = 8;
		RequestGridReconfigure(256, 256, 0.10f, 40);
		break;
	case GpuFluidStressPreset::Heavy:
		syntheticEmitterCount_ = 24;
		syntheticObstacleCount_ = 24;
		RequestGridReconfigure(512, 512, 0.075f, 60);
		break;
	case GpuFluidStressPreset::Extreme:
		syntheticEmitterCount_ = 64;
		syntheticObstacleCount_ = 64;
		RequestGridReconfigure(1024, 1024, 0.05f, 80);
		break;
	case GpuFluidStressPreset::Off:
	default:
		syntheticEmitterCount_ = 0;
		syntheticObstacleCount_ = 0;
		break;
	}
}

void GpuFluidManager::SetSyntheticStressCounts(uint32_t emitterCount, uint32_t obstacleCount)
{
	syntheticEmitterCount_ = (std::min)(emitterCount, 256u);
	syntheticObstacleCount_ = (std::min)(obstacleCount, 256u);
	stressPreset_ = GpuFluidStressPreset::Off;
	if (syntheticEmitterCount_ > 0)
	{
		simulationActive_ = true;
	}
}

bool GpuFluidManager::ApplyPendingReconfigure()
{
	if (!pendingReconfigure_.pending)
	{
		return true;
	}

	const PendingReconfigure request = pendingReconfigure_;
	pendingReconfigure_.pending = false;
	return RecreateGrid(request.grid, request.pressureIterations);
}

bool GpuFluidManager::RecreateGrid(const GpuFluidGridDesc& gridDesc, uint32_t pressureIterations)
{
	if (!gridDesc.IsValid() || pressureIterations == 0)
	{
		return false;
	}

	DirectXCommon* dxCommon = DirectXCommon::GetInstance();
	if (dxCommon == nullptr || dxCommon->GetFenceManager() == nullptr)
	{
		return false;
	}

	// 解像度変更は稀なEditor操作なので、直前までにSubmit済みのGPU参照だけを完了させてからTextureを再生成する。
	DX12FenceManager* fence = dxCommon->GetFenceManager();
	fence->WaitForValue(fence->GetCurrentValue());

	const GpuFluidGridDesc oldGridDesc = simulationDesc_.grid;
	const uint32_t oldPressureIterations = simulationDesc_.pressureIterations;
	const Vector3 oldDomainOrigin = domain_.origin;
	const Vector3 axisU = Vector3::NormalizeSafe(domain_.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain_.axisV, { 0.0f, 1.0f, 0.0f });
	const Vector3 domainCenter = oldDomainOrigin +
		axisU * (static_cast<float>(oldGridDesc.width) * oldGridDesc.cellSize * 0.5f) +
		axisV * (static_cast<float>(oldGridDesc.height) * oldGridDesc.cellSize * 0.5f);

	grid_.Finalize();
	simulationDesc_.grid = gridDesc;
	simulationDesc_.pressureIterations = pressureIterations;
	domain_.origin = domainCenter -
		axisU * (static_cast<float>(gridDesc.width) * gridDesc.cellSize * 0.5f) -
		axisV * (static_cast<float>(gridDesc.height) * gridDesc.cellSize * 0.5f); // Stress解像度変更でもWorld-space Domain中心を固定する。

	if (!grid_.Initialize(gridDesc) || !ResetSimulation())
	{
		grid_.Finalize();
		simulationDesc_.grid = oldGridDesc;
		simulationDesc_.pressureIterations = oldPressureIterations;
		domain_.origin = oldDomainOrigin;
		return grid_.Initialize(oldGridDesc) && ResetSimulation();
	}
	return true;
}

bool GpuFluidManager::ResetSimulation()
{
	if (!initialized_ || !grid_.IsInitialized() || !resetPass_.Reset(grid_))
	{
		return false;
	}

	accumulatorSeconds_ = 0.0f;
	elapsedSimulationSeconds_ = 0.0f;
	simulationActive_ = syntheticEmitterCount_ > 0;
	return true;
}

bool GpuFluidManager::ExecuteSimulationStep(float deltaTime)
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

	// Source/Force後に再ProjectionするPhase16の最終固定Step順をRuntime側で一箇所に固定する。
	elapsedSimulationSeconds_ += deltaTime;
	++stats_.totalSimulationSteps;
	return true;
}

void GpuFluidManager::CollectSceneSources(const ActorWorld& world)
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

			const GpuFluidEmitterSource source = emitter->BuildEmitterSource();
			if (source.enabled && source.IsValid())
			{
				emitters_.push_back(source);
			}
		}
	}

	GpuFluidColliderObstacleAdapter::CollectSources(world, obstacles_);
}

void GpuFluidManager::AppendSyntheticStressSources()
{
	const uint32_t sceneEmitterCount = static_cast<uint32_t>(emitters_.size());
	const uint32_t sceneObstacleCount = static_cast<uint32_t>(obstacles_.size());
	const GpuFluidGridDesc& gridDesc = simulationDesc_.grid;
	const Vector3 axisU = Vector3::NormalizeSafe(domain_.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain_.axisV, { 0.0f, 1.0f, 0.0f });
	const float widthWorld = static_cast<float>(gridDesc.width) * gridDesc.cellSize;
	const float heightWorld = static_cast<float>(gridDesc.height) * gridDesc.cellSize;
	const float minExtent = (std::min)(widthWorld, heightWorld);

	if (syntheticEmitterCount_ > 0)
	{
		const uint32_t columns = (std::max)(1u, static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(syntheticEmitterCount_)))));
		for (uint32_t index = 0; index < syntheticEmitterCount_; ++index)
		{
			const uint32_t column = index % columns;
			const uint32_t row = index / columns;
			const float u = (static_cast<float>(column) + 0.5f) / static_cast<float>(columns);
			const float v = 0.08f + 0.05f * static_cast<float>(row % 4u);

			GpuFluidEmitterSource source{};
			source.worldPosition = domain_.origin + axisU * (widthWorld * u) + axisV * (heightWorld * v);
			source.worldVelocity = axisV * 2.5f;
			source.radius = (std::max)(gridDesc.cellSize * 2.0f, minExtent * 0.018f);
			source.velocityStrength = 1.5f;
			source.densityRate = 4.0f;
			source.temperatureRate = 3.0f;
			source.falloffExponent = 2.0f;
			emitters_.push_back(source);
		}
	}

	if (syntheticObstacleCount_ > 0)
	{
		for (uint32_t index = 0; index < syntheticObstacleCount_; ++index)
		{
			const float u = (static_cast<float>(index) + 0.5f) / static_cast<float>(syntheticObstacleCount_);
			const float v = 0.35f + 0.25f * static_cast<float>(index & 1u);
			GpuFluidObstacleSource obstacle{};
			obstacle.shape = GpuFluidObstacleShape::Sphere;
			obstacle.worldCenter = domain_.origin + axisU * (widthWorld * u) + axisV * (heightWorld * v);
			obstacle.radius = (std::max)(gridDesc.cellSize * 1.5f, minExtent * 0.012f);
			obstacles_.push_back(obstacle);
		}
	}

	stats_.sceneEmitterCount = sceneEmitterCount;
	stats_.sceneObstacleCount = sceneObstacleCount;
}

void GpuFluidManager::RefreshStats(uint32_t substeps, bool lastStepSucceeded)
{
	stats_.lastFrameSubsteps = substeps;
	stats_.syntheticEmitterCount = syntheticEmitterCount_;
	stats_.syntheticObstacleCount = syntheticObstacleCount_;
	stats_.approximateGpuMemoryBytes = grid_.IsInitialized() ? grid_.GetApproximateGpuMemoryBytes() : 0;
	stats_.resetCount = resetPass_.GetResetCount();
	stats_.velocityDispatchCount = velocityAdvectionPass_.GetDispatchCount();
	stats_.pressureDispatchCount = pressureProjectionPass_.GetDispatchCount();
	stats_.scalarDispatchCount = scalarAdvectionPass_.GetDispatchCount();
	stats_.forceDispatchCount = forcePass_.GetDispatchCount();
	stats_.emitterDispatchCount = emitterInjectionPass_.GetDispatchCount();
	stats_.obstacleDispatchCount = obstacleRasterPass_.GetDispatchCount();
	stats_.forwardDrawCount = forwardRenderer_.GetDrawCount();
	stats_.accumulatorSeconds = accumulatorSeconds_;
	stats_.elapsedSimulationSeconds = elapsedSimulationSeconds_;
	stats_.simulationActive = simulationActive_;
	stats_.lastStepSucceeded = lastStepSucceeded;
}

} // namespace Ken4lowEngine
