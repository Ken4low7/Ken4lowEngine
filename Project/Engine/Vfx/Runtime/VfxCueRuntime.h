#pragma once

#include "VfxCueCompiler.h"
#include "VfxRuntimeTypes.h"
#include "Adapters/VfxTrackAdapters.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ken4lowEngine
{

class ActorWorld;

/// <summary>
/// GameplayからCue名/Handleだけで複数VFX Subsystemを同期再生するRuntime Facade。
/// </summary>
class VfxCueRuntime
{
public:
	static VfxCueRuntime* GetInstance();

	bool Initialize();
	void Finalize();
	void BeginFrame();
	void Update(float deltaTime, ActorWorld* world);

	bool RegisterCue(const VfxCueDesc& desc, const std::string& sourcePath = {});
	bool LoadCue(const std::string& filePath);
	bool ReloadCue(const std::string& cueName);
	bool UnregisterCue(const std::string& cueName);

	VfxCueHandle Play(const std::string& cueName, const Vector3& worldPosition = {});
	bool Stop(VfxCueHandle handle);
	uint32_t StopAll(const std::string& cueName);
	void StopAll();
	bool SetWorldPosition(VfxCueHandle handle, const Vector3& worldPosition);
	bool SetFloatParameter(VfxCueHandle handle, const std::string& parameterName, float value);

	uint32_t RunStressBurst(
		const std::string& cueName,
		uint32_t count,
		const Vector3& center,
		float spacing = 1.5f);

	[[nodiscard]] bool IsCueRegistered(const std::string& cueName) const;
	[[nodiscard]] bool IsHandleActive(VfxCueHandle handle) const;
	[[nodiscard]] const VfxCueDesc* GetRegisteredCueDesc(const std::string& cueName) const;
	[[nodiscard]] const VfxCueProgram* GetRegisteredProgram(const std::string& cueName) const;
	[[nodiscard]] const std::string* GetSourcePath(const std::string& cueName) const;
	[[nodiscard]] const VfxRuntimeStats& GetStats() const { return stats_; }
	[[nodiscard]] VfxRuntimeBudget& GetEditableBudget() { return budget_; }
	[[nodiscard]] const VfxRuntimeBudget& GetBudget() const { return budget_; }

private:
	struct RegisteredCue
	{
		VfxCueDesc desc{};
		VfxCueProgram program{};
		std::string sourcePath;
	};

	struct ActiveTrack
	{
		uint32_t instructionIndex = 0;
		uint64_t runtimeTrackId = 0;
		VfxTrackRuntimeToken token{};
	};

	struct Instance
	{
		VfxCueHandle handle{};
		std::string cueName;
		VfxCueProgram program{};
		Vector3 worldPosition{};
		float elapsed = 0.0f;
		uint32_t nextInstructionIndex = 0;
		bool justStarted = true;
		std::unordered_map<std::string, float> parameters;
		std::vector<ActiveTrack> activeTracks;
	};

	VfxCueRuntime() = default;
	~VfxCueRuntime() = default;
	VfxCueRuntime(const VfxCueRuntime&) = delete;
	VfxCueRuntime& operator=(const VfxCueRuntime&) = delete;

	void SwitchActiveWorld(ActorWorld* world);
	bool AdvanceInstance(Instance& instance, float deltaTime, ActorWorld* world);
	bool StartDueTracks(Instance& instance, ActorWorld* world);
	bool StartTrack(Instance& instance, uint32_t instructionIndex, ActorWorld* world);
	void UpdateActiveTracks(Instance& instance);
	void StopExpiredTracks(Instance& instance);
	void StopInstanceTracks(Instance& instance);
	VfxTrackStartContext BuildTrackContext(const Instance& instance, uint32_t instructionIndex, uint64_t runtimeTrackId) const;
	VfxResolvedTrackParameters ResolveTrackParameters(const Instance& instance, const VfxCueInstruction& instruction) const;
	void RefreshStats();
	void SetStatus(bool success, std::string message);
	VfxCueHandle AllocateHandle();
	uint64_t AllocateTrackId();

private:
	std::unordered_map<std::string, RegisteredCue> cues_;
	std::unordered_map<uint64_t, Instance> instances_;
	VfxTrackAdapterSet adapters_{};
	VfxRuntimeBudget budget_{};
	VfxRuntimeStats stats_{};
	ActorWorld* activeWorld_ = nullptr; // pointer値だけでWorld切替を検出し、破棄済みWorldはdereferenceしない。
	uint64_t nextHandleValue_ = 1;
	uint64_t nextTrackId_ = 1;
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
