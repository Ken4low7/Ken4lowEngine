#pragma once

#include "Engine/Vfx/Graph/Asset/VfxGraphSerializer.h"
#include "Engine/Vfx/Graph/Runtime/VfxGraphCompiler.h"
#include "Engine/Graphics/Renderer/GpuParticle/Runtime/GpuParticleEffectRuntime.h"
#include "Engine/Vfx/Runtime/VfxRuntimeTypes.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{

struct VfxGraphPlayHandle
{
	GpuParticleEffectRuntime::PlayHandle particleHandle{};
	VfxCueHandle integrationHandle{};
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
	uint64_t integrationStarts = 0u;
	uint64_t integrationStops = 0u;
	uint64_t integrationFailures = 0u;
	uint64_t culledOneShots = 0u;
	uint64_t budgetRejectedPlays = 0u;
	uint64_t lodNearSelections = 0u;
	uint64_t lodMidSelections = 0u;
	uint64_t lodFarSelections = 0u;
	uint64_t loopScaleChanges = 0u;
	uint64_t loopCullTransitions = 0u;
	uint32_t graphStartCostThisFrame = 0u;
	uint32_t activeLoopCount = 0u;
	uint32_t activeLoopCost = 0u;
};

/// <summary>
/// Niagara-like Graph AssetをCompileし、粒子はPhase13、Subsystem統合OutputはPhase18 Runtimeへ渡すFacade。
/// Phase27は既存Camera/Frustum/Budgetを再利用し、Bounds/LOD/Cullingをここで統合する。
/// </summary>
class VfxGraphRuntime
{
public:
	static VfxGraphRuntime* GetInstance();

	void BeginFrame();
	void UpdateScalability();

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
	struct ActiveLoopScalability
	{
		VfxGraphPlayHandle handle{};
		Vector3 worldPosition{};
		float runtimeScale = 1.0f;
		bool culled = false;
		uint32_t budgetCost = 1u;
	};

	VfxGraphRuntime() = default;
	float EvaluateRuntimeScale(const VfxGraphProgram& program, const Vector3& worldPosition, bool& outCulled, float& outDistance) const;
	bool ReserveStartBudget(const VfxGraphProgram& program, bool loopStart);
	void RecordLodSelection(const VfxGraphProgram& program, float distance, float scale);
	void RefreshLoopStats();
	void StopActiveLoopsForGraph(const std::string& graphName);
	void SetStatus(bool success, std::string message);

	std::unordered_map<std::string, VfxGraphProgram> programs_;
	std::unordered_map<std::string, std::string> sourcePaths_;
	std::unordered_map<uint32_t, ActiveLoopScalability> activeLoops_;
	VfxGraphRuntimeStats stats_{};
	bool lastOperationSucceeded_ = true;
	std::string lastStatus_ = "VFX Graph Runtime ready.";
};

} // namespace Ken4lowEngine
