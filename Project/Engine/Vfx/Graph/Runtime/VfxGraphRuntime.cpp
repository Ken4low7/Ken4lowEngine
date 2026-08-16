#include "VfxGraphRuntime.h"

#include "Engine/Graphics/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Culling/Frustum.h"
#include "Engine/Vfx/Runtime/VfxCueRuntime.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
namespace
{
	Vector3 Add(const Vector3& a, const Vector3& b)
	{
		return { a.x + b.x, a.y + b.y, a.z + b.z };
	}

	float Distance(const Vector3& a, const Vector3& b)
	{
		const float x = a.x - b.x;
		const float y = a.y - b.y;
		const float z = a.z - b.z;
		return std::sqrt(x * x + y * y + z * z);
	}
}

VfxGraphRuntime* VfxGraphRuntime::GetInstance()
{
	static VfxGraphRuntime instance;
	return &instance;
}

void VfxGraphRuntime::BeginFrame()
{
	stats_.graphStartCostThisFrame = 0u;
}

void VfxGraphRuntime::UpdateScalability()
{
	for (auto& [particleHandleId, state] : activeLoops_)
	{
		(void)particleHandleId;
		const VfxGraphProgram* program = GetProgram(state.handle.graphName);
		if (program == nullptr) continue;

		bool culled = false;
		float distance = 0.0f;
		const float scale = EvaluateRuntimeScale(*program, state.worldPosition, culled, distance);
		if (std::abs(scale - state.runtimeScale) > 0.001f)
		{
			if (GpuParticleEffectRuntime::GetInstance()->SetLoopRuntimeScale(state.handle.particleHandle, scale))
			{
				if (state.handle.integrationHandle.IsValid())
				{
					VfxCueRuntime::GetInstance()->SetRuntimeScale(state.handle.integrationHandle, scale);
				}
				++stats_.loopScaleChanges;
				RecordLodSelection(*program, distance, scale);
			}
		}
		if (culled != state.culled)
		{
			++stats_.loopCullTransitions;
		}
		state.runtimeScale = scale;
		state.culled = culled;
	}
	RefreshLoopStats();
}

bool VfxGraphRuntime::RegisterGraph(const VfxGraphDesc& graph)
{
	VfxGraphCompileResult compiled = VfxGraphCompiler::Compile(graph);
	if (!compiled.success)
	{
		++stats_.compileFailures;
		SetStatus(false, compiled.errors.empty() ? "VFX Graph compile failed." : compiled.errors.front());
		return false;
	}

	StopActiveLoopsForGraph(graph.graphName);
	if (!GpuParticleEffectRuntime::GetInstance()->RegisterEffect(compiled.program.particleEffect))
	{
		SetStatus(false, "Phase13 GPU Particle Runtime rejected graph: " + graph.graphName);
		return false;
	}

	VfxCueRuntime* cueRuntime = VfxCueRuntime::GetInstance();
	const auto previous = programs_.find(graph.graphName);
	if (compiled.program.HasIntegrationTracks())
	{
		if (!cueRuntime->RegisterCue(compiled.program.integrationOneShotCue) || !cueRuntime->RegisterCue(compiled.program.integrationLoopCue))
		{
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 VFX Runtime rejected graph integrations: " + graph.graphName);
			return false;
		}
	}
	else if (previous != programs_.end() && previous->second.HasIntegrationTracks())
	{
		cueRuntime->UnregisterCue(previous->second.integrationOneShotCue.cueName);
		cueRuntime->UnregisterCue(previous->second.integrationLoopCue.cueName);
	}

	const bool replacing = previous != programs_.end();
	programs_[graph.graphName] = std::move(compiled.program);
	if (!replacing) ++stats_.registeredGraphs;
	SetStatus(true, "Registered VFX Graph: " + graph.graphName);
	return true;
}

bool VfxGraphRuntime::LoadGraph(const std::string& filePath)
{
	VfxGraphDesc graph{};
	if (!VfxGraphSerializer::Load(graph, filePath))
	{
		SetStatus(false, "Failed to load VFX Graph: " + filePath);
		return false;
	}
	const std::string graphName = graph.graphName;
	if (!RegisterGraph(graph)) return false;
	sourcePaths_[graphName] = filePath;
	SetStatus(true, "Loaded VFX Graph: " + graphName);
	return true;
}

bool VfxGraphRuntime::ReloadGraph(const std::string& graphName)
{
	const auto it = sourcePaths_.find(graphName);
	if (it == sourcePaths_.end())
	{
		SetStatus(false, "Reload failed because source path is not registered: " + graphName);
		return false;
	}
	const std::string path = it->second;
	if (!LoadGraph(path)) return false;
	++stats_.reloads;
	SetStatus(true, "Reloaded VFX Graph: " + graphName);
	return true;
}

bool VfxGraphRuntime::Play(const std::string& graphName, const Vector3& worldPosition)
{
	++stats_.playRequests;
	const VfxGraphProgram* program = GetProgram(graphName);
	if (program == nullptr)
	{
		SetStatus(false, "Play failed because graph is not registered: " + graphName);
		return false;
	}
	if (!ReserveStartBudget(*program, false)) return false;

	bool culled = false;
	float distance = 0.0f;
	const float runtimeScale = EvaluateRuntimeScale(*program, worldPosition, culled, distance);
	if (culled)
	{
		++stats_.culledOneShots;
		SetStatus(false, "VFX Graph one-shot culled by Phase27 visibility policy: " + graphName);
		return false;
	}
	RecordLodSelection(*program, distance, runtimeScale);

	VfxCueHandle integrationHandle{};
	if (program->HasIntegrationTracks())
	{
		integrationHandle = VfxCueRuntime::GetInstance()->Play(program->integrationOneShotCue.cueName, worldPosition);
		if (!integrationHandle.IsValid() || !VfxCueRuntime::GetInstance()->SetRuntimeScale(integrationHandle, runtimeScale))
		{
			if (integrationHandle.IsValid()) VfxCueRuntime::GetInstance()->Stop(integrationHandle);
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 runtime failed to play graph integrations: " + graphName);
			return false;
		}
		++stats_.integrationStarts;
	}

	const bool success = GpuParticleEffectRuntime::GetInstance()->Play(graphName, worldPosition, runtimeScale);
	if (!success && integrationHandle.IsValid())
	{
		VfxCueRuntime::GetInstance()->Stop(integrationHandle);
		++stats_.integrationStops;
	}
	if (success) ++stats_.playSuccesses;
	SetStatus(success, success ? "Played VFX Graph: " + graphName : "Phase13 runtime failed to play graph: " + graphName);
	return success;
}

VfxGraphPlayHandle VfxGraphRuntime::PlayLoop(const std::string& graphName, const Vector3& worldPosition)
{
	const VfxGraphProgram* program = GetProgram(graphName);
	if (program == nullptr)
	{
		SetStatus(false, "PlayLoop failed because graph is not registered: " + graphName);
		return {};
	}
	if (!ReserveStartBudget(*program, true)) return {};

	bool culled = false;
	float distance = 0.0f;
	const float runtimeScale = EvaluateRuntimeScale(*program, worldPosition, culled, distance);
	RecordLodSelection(*program, distance, runtimeScale);

	const GpuParticleEffectRuntime::PlayHandle particleHandle = GpuParticleEffectRuntime::GetInstance()->PlayLoop(graphName, worldPosition, runtimeScale);
	if (!particleHandle.IsValid())
	{
		SetStatus(false, "Phase13 runtime failed to start loop graph: " + graphName);
		return {};
	}

	VfxCueHandle integrationHandle{};
	if (program->HasIntegrationTracks())
	{
		integrationHandle = VfxCueRuntime::GetInstance()->Play(program->integrationLoopCue.cueName, worldPosition);
		if (!integrationHandle.IsValid() || !VfxCueRuntime::GetInstance()->SetRuntimeScale(integrationHandle, runtimeScale))
		{
			GpuParticleEffectRuntime::GetInstance()->StopLoop(particleHandle);
			if (integrationHandle.IsValid()) VfxCueRuntime::GetInstance()->Stop(integrationHandle);
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 runtime failed to start loop graph integrations: " + graphName);
			return {};
		}
		++stats_.integrationStarts;
	}

	VfxGraphPlayHandle handle{ particleHandle, integrationHandle, graphName };
	activeLoops_[particleHandle.id] = { handle, worldPosition, runtimeScale, culled, program->scalability.budgetCost };
	++stats_.loopStarts;
	RefreshLoopStats();
	SetStatus(true, "Looped VFX Graph: " + graphName);
	return handle;
}

bool VfxGraphRuntime::StopLoop(VfxGraphPlayHandle handle)
{
	if (!handle.IsValid())
	{
		SetStatus(false, "StopLoop received invalid graph handle.");
		return false;
	}
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->StopLoop(handle.particleHandle);
	bool integrationSuccess = true;
	if (handle.integrationHandle.IsValid())
	{
		integrationSuccess = VfxCueRuntime::GetInstance()->Stop(handle.integrationHandle);
		if (integrationSuccess) ++stats_.integrationStops;
		else ++stats_.integrationFailures;
	}
	activeLoops_.erase(handle.particleHandle.id);
	const bool success = particleSuccess && integrationSuccess;
	if (success) ++stats_.loopStops;
	RefreshLoopStats();
	SetStatus(success, success ? "Stopped VFX Graph loop: " + handle.graphName : "Failed to stop VFX Graph loop: " + handle.graphName);
	return success;
}

bool VfxGraphRuntime::SetLoopPosition(VfxGraphPlayHandle handle, const Vector3& worldPosition)
{
	if (!handle.IsValid()) return false;
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->SetLoopPosition(handle.particleHandle, worldPosition);
	const bool integrationSuccess = !handle.integrationHandle.IsValid() || VfxCueRuntime::GetInstance()->SetWorldPosition(handle.integrationHandle, worldPosition);
	const auto it = activeLoops_.find(handle.particleHandle.id);
	if (it != activeLoops_.end()) it->second.worldPosition = worldPosition;
	return particleSuccess && integrationSuccess;
}

bool VfxGraphRuntime::SetFloatParameter(const std::string& graphName, const std::string& parameterName, float value)
{
	if (!IsRegistered(graphName)) return false;
	return GpuParticleEffectRuntime::GetInstance()->SetFloatParameter(graphName, parameterName, value);
}

bool VfxGraphRuntime::SetFloatParameter(VfxGraphPlayHandle handle, const std::string& parameterName, float value)
{
	if (!handle.IsValid()) return false;
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->SetFloatParameter(handle.particleHandle, parameterName, value);
	const bool integrationSuccess = !handle.integrationHandle.IsValid() || VfxCueRuntime::GetInstance()->SetFloatParameter(handle.integrationHandle, parameterName, value);
	return particleSuccess && integrationSuccess;
}

bool VfxGraphRuntime::IsRegistered(const std::string& graphName) const
{
	return programs_.contains(graphName);
}

const VfxGraphProgram* VfxGraphRuntime::GetProgram(const std::string& graphName) const
{
	const auto it = programs_.find(graphName);
	return it == programs_.end() ? nullptr : &it->second;
}

float VfxGraphRuntime::EvaluateRuntimeScale(const VfxGraphProgram& program, const Vector3& worldPosition, bool& outCulled, float& outDistance) const
{
	outCulled = false;
	outDistance = 0.0f;
	CameraManager* cameraManager = CameraManager::GetInstance();
	if (cameraManager->GetMainCamera() == nullptr && !cameraManager->HasRenderViewOverride()) return 1.0f;

	BoundingSphere worldBounds = program.localBounds;
	worldBounds.center = Add(worldPosition, program.localBounds.center);
	const Vector3 cameraPosition = cameraManager->GetActiveCameraPosition();
	outDistance = Distance(cameraPosition, worldBounds.center);

	if (program.scalability.maxDrawDistance > 0.0f && outDistance - worldBounds.radius > program.scalability.maxDrawDistance)
	{
		outCulled = true;
		return 0.0f;
	}
	if (program.scalability.frustumCulling)
	{
		Frustum frustum{};
		frustum.BuildFromViewProjection(cameraManager->GetActiveViewProjectionMatrix());
		if (!frustum.Intersects(worldBounds))
		{
			outCulled = true;
			return 0.0f;
		}
	}

	if (outDistance <= program.scalability.lodNearDistance) return 1.0f;
	if (outDistance <= program.scalability.lodFarDistance) return program.scalability.lodMidScale;
	return program.scalability.lodFarScale;
}

bool VfxGraphRuntime::ReserveStartBudget(const VfxGraphProgram& program, bool loopStart)
{
	const VfxRuntimeBudget& budget = VfxCueRuntime::GetInstance()->GetBudget();
	const uint32_t cost = (std::max)(program.scalability.budgetCost, 1u);
	if (stats_.graphStartCostThisFrame + cost > budget.maxVfxGraphStartCostPerFrame)
	{
		++stats_.budgetRejectedPlays;
		SetStatus(false, "VFX Graph rejected by per-frame start budget: " + program.graphName);
		return false;
	}
	if (loopStart && stats_.activeLoopCost + cost > budget.maxActiveVfxGraphLoopCost)
	{
		++stats_.budgetRejectedPlays;
		SetStatus(false, "VFX Graph loop rejected by active graph budget: " + program.graphName);
		return false;
	}
	stats_.graphStartCostThisFrame += cost;
	return true;
}

void VfxGraphRuntime::RecordLodSelection(const VfxGraphProgram& program, float distance, float scale)
{
	if (scale <= 0.0f) return;
	if (distance <= program.scalability.lodNearDistance) ++stats_.lodNearSelections;
	else if (distance <= program.scalability.lodFarDistance) ++stats_.lodMidSelections;
	else ++stats_.lodFarSelections;
}

void VfxGraphRuntime::RefreshLoopStats()
{
	stats_.activeLoopCount = static_cast<uint32_t>(activeLoops_.size());
	stats_.activeLoopCost = 0u;
	for (const auto& [id, state] : activeLoops_)
	{
		(void)id;
		stats_.activeLoopCost += state.budgetCost;
	}
}

void VfxGraphRuntime::StopActiveLoopsForGraph(const std::string& graphName)
{
	std::vector<VfxGraphPlayHandle> handles;
	for (const auto& [id, state] : activeLoops_)
	{
		(void)id;
		if (state.handle.graphName == graphName) handles.push_back(state.handle);
	}
	for (const VfxGraphPlayHandle& handle : handles) StopLoop(handle);
}

void VfxGraphRuntime::SetStatus(bool success, std::string message)
{
	lastOperationSucceeded_ = success;
	lastStatus_ = std::move(message);
}

} // namespace Ken4lowEngine
