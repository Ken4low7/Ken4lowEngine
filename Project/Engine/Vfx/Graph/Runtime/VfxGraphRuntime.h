#pragma once

#include "Engine/Vfx/Graph/Asset/VfxGraphSerializer.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphCompiler.h"
#include "Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h"

#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{

struct VfxGraphPlayHandle
{
	GpuParticleEffectRuntime::PlayHandle particleHandle{};
	std::string graphName;

	[[nodiscard]] bool IsValid() const
	{
		return particleHandle.IsValid();
	}
};

struct VfxGraphRuntimeStats
{
	uint64_t registeredGraphs = 0u;
	uint64_t compileFailures = 0u;
	uint64_t playRequests = 0u;
	uint64_t playSuccesses = 0u;
	uint64_t loopStarts = 0u;
	uint64_t loopStops = 0u;
	uint64_t reloads = 0u;
};

/// <summary>
/// Niagara-like Graph AssetをCompileし、既存Phase13 GPU Particle Runtimeへ渡すFacade。
/// GPU Particle backendの所有権は移さず、Graph側はAuthoring/Compile責務だけを持つ。
/// </summary>
class VfxGraphRuntime
{
public:
	static VfxGraphRuntime* GetInstance();

	bool RegisterGraph(const VfxGraphDesc& graph);
	bool LoadGraph(const std::string& filePath);
	bool ReloadGraph(const std::string& graphName);

	bool Play(const std::string& graphName, const Vector3& worldPosition);
	VfxGraphPlayHandle PlayLoop(const std::string& graphName, const Vector3& worldPosition);
	bool StopLoop(VfxGraphPlayHandle handle);
	bool SetLoopPosition(VfxGraphPlayHandle handle, const Vector3& worldPosition);
	bool SetFloatParameter(const std::string& graphName, const std::string& parameterName, float value);
	bool SetFloatParameter(VfxGraphPlayHandle handle, const std::string& parameterName, float value);

	[[nodiscard]] bool IsRegistered(const std::string& graphName) const;
	[[nodiscard]] const VfxGraphProgram* GetProgram(const std::string& graphName) const;
	[[nodiscard]] const VfxGraphRuntimeStats& GetStats() const { return stats_; }
	[[nodiscard]] bool WasLastOperationSuccessful() const { return lastOperationSucceeded_; }
	[[nodiscard]] const std::string& GetLastStatus() const { return lastStatus_; }

private:
	VfxGraphRuntime() = default;
	void SetStatus(bool success, std::string message);

	std::unordered_map<std::string, VfxGraphProgram> programs_;
	std::unordered_map<std::string, std::string> sourcePaths_;
	VfxGraphRuntimeStats stats_{};
	bool lastOperationSucceeded_ = true;
	std::string lastStatus_ = "VFX Graph Runtime ready.";
};

} // namespace Ken4lowEngine
