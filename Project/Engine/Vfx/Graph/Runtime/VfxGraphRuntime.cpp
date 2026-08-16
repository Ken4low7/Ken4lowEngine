#include "VfxGraphRuntime.h"

#include "Engine/Vfx/Runtime/VfxCueRuntime.h"

#include <utility>

namespace Ken4lowEngine
{

VfxGraphRuntime* VfxGraphRuntime::GetInstance()
{
	static VfxGraphRuntime instance;
	return &instance;
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
		// Hot reloadでOutputが消えた時は古いgenerated Cueを残さない。
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

	VfxCueHandle integrationHandle{};
	if (program->HasIntegrationTracks())
	{
		integrationHandle = VfxCueRuntime::GetInstance()->Play(program->integrationOneShotCue.cueName, worldPosition);
		if (!integrationHandle.IsValid())
		{
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 runtime failed to play graph integrations: " + graphName);
			return false;
		}
		++stats_.integrationStarts;
	}

	const bool success = GpuParticleEffectRuntime::GetInstance()->Play(graphName, worldPosition);
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

	const GpuParticleEffectRuntime::PlayHandle particleHandle = GpuParticleEffectRuntime::GetInstance()->PlayLoop(graphName, worldPosition);
	if (!particleHandle.IsValid())
	{
		SetStatus(false, "Phase13 runtime failed to start loop graph: " + graphName);
		return {};
	}

	VfxCueHandle integrationHandle{};
	if (program->HasIntegrationTracks())
	{
		integrationHandle = VfxCueRuntime::GetInstance()->Play(program->integrationLoopCue.cueName, worldPosition);
		if (!integrationHandle.IsValid())
		{
			GpuParticleEffectRuntime::GetInstance()->StopLoop(particleHandle);
			++stats_.integrationFailures;
			SetStatus(false, "Phase18 runtime failed to start loop graph integrations: " + graphName);
			return {};
		}
		++stats_.integrationStarts;
	}

	++stats_.loopStarts;
	SetStatus(true, "Looped VFX Graph: " + graphName);
	return { particleHandle, integrationHandle, graphName };
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
	const bool success = particleSuccess && integrationSuccess;
	if (success) ++stats_.loopStops;
	SetStatus(success, success ? "Stopped VFX Graph loop: " + handle.graphName : "Failed to stop VFX Graph loop: " + handle.graphName);
	return success;
}

bool VfxGraphRuntime::SetLoopPosition(VfxGraphPlayHandle handle, const Vector3& worldPosition)
{
	if (!handle.IsValid()) return false;
	const bool particleSuccess = GpuParticleEffectRuntime::GetInstance()->SetLoopPosition(handle.particleHandle, worldPosition);
	const bool integrationSuccess = !handle.integrationHandle.IsValid() || VfxCueRuntime::GetInstance()->SetWorldPosition(handle.integrationHandle, worldPosition);
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

void VfxGraphRuntime::SetStatus(bool success, std::string message)
{
	lastOperationSucceeded_ = success;
	lastStatus_ = std::move(message);
}

} // namespace Ken4lowEngine
