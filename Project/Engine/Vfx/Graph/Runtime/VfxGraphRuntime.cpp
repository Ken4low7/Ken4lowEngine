#include "VfxGraphRuntime.h"

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

	const bool replacing = programs_.contains(graph.graphName);
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
	if (!IsRegistered(graphName))
	{
		SetStatus(false, "Play failed because graph is not registered: " + graphName);
		return false;
	}
	const bool success = GpuParticleEffectRuntime::GetInstance()->Play(graphName, worldPosition);
	if (success) ++stats_.playSuccesses;
	SetStatus(success, success ? "Played VFX Graph: " + graphName : "Phase13 runtime failed to play graph: " + graphName);
	return success;
}

VfxGraphPlayHandle VfxGraphRuntime::PlayLoop(const std::string& graphName, const Vector3& worldPosition)
{
	if (!IsRegistered(graphName))
	{
		SetStatus(false, "PlayLoop failed because graph is not registered: " + graphName);
		return {};
	}
	const GpuParticleEffectRuntime::PlayHandle handle = GpuParticleEffectRuntime::GetInstance()->PlayLoop(graphName, worldPosition);
	if (!handle.IsValid())
	{
		SetStatus(false, "Phase13 runtime failed to start loop graph: " + graphName);
		return {};
	}
	++stats_.loopStarts;
	SetStatus(true, "Looped VFX Graph: " + graphName);
	return { handle, graphName };
}

bool VfxGraphRuntime::StopLoop(VfxGraphPlayHandle handle)
{
	if (!handle.IsValid())
	{
		SetStatus(false, "StopLoop received invalid graph handle.");
		return false;
	}
	const bool success = GpuParticleEffectRuntime::GetInstance()->StopLoop(handle.particleHandle);
	if (success) ++stats_.loopStops;
	SetStatus(success, success ? "Stopped VFX Graph loop: " + handle.graphName : "Failed to stop VFX Graph loop: " + handle.graphName);
	return success;
}

bool VfxGraphRuntime::SetLoopPosition(VfxGraphPlayHandle handle, const Vector3& worldPosition)
{
	if (!handle.IsValid()) return false;
	return GpuParticleEffectRuntime::GetInstance()->SetLoopPosition(handle.particleHandle, worldPosition);
}

bool VfxGraphRuntime::SetFloatParameter(const std::string& graphName, const std::string& parameterName, float value)
{
	if (!IsRegistered(graphName)) return false;
	return GpuParticleEffectRuntime::GetInstance()->SetFloatParameter(graphName, parameterName, value);
}

bool VfxGraphRuntime::SetFloatParameter(VfxGraphPlayHandle handle, const std::string& parameterName, float value)
{
	if (!handle.IsValid()) return false;
	return GpuParticleEffectRuntime::GetInstance()->SetFloatParameter(handle.particleHandle, parameterName, value);
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
