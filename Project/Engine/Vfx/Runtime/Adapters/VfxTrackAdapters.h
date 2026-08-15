#pragma once

#include "../VfxRuntimeTypes.h"
#include "ActorHandle.h"
#include "GpuParticleEffectRuntime.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Ken4lowEngine
{

class ActorWorld;

class VfxParticleTrackAdapter
{
public:
	bool Start(const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken);
	bool Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime);
	void Stop(VfxTrackRuntimeToken token);
	void StopAll();
	[[nodiscard]] uint32_t GetActiveCount() const { return static_cast<uint32_t>(entries_.size()); }

private:
	struct Entry
	{
		bool looping = false;
		std::string effectName;
		GpuParticleEffectRuntime::PlayHandle particleHandle{};
	};
	std::unordered_map<uint64_t, Entry> entries_;
};

class VfxFluidTrackAdapter
{
public:
	bool Start(ActorWorld* world, const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken);
	bool Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime);
	void Stop(VfxTrackRuntimeToken token);
	void StopAll();
	void AbandonWorld(const ActorWorld* world);
	[[nodiscard]] uint32_t GetActiveCount() const { return static_cast<uint32_t>(entries_.size()); }

private:
	struct Entry
	{
		ActorWorld* world = nullptr;
		ActorHandle actor{};
	};
	std::unordered_map<uint64_t, Entry> entries_;
};

class VfxLightTrackAdapter
{
public:
	bool Start(ActorWorld* world, const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken);
	bool Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime);
	void Stop(VfxTrackRuntimeToken token);
	void StopAll();
	void AbandonWorld(const ActorWorld* world);
	[[nodiscard]] uint32_t GetActiveCount() const { return static_cast<uint32_t>(entries_.size()); }

private:
	struct Entry
	{
		ActorWorld* world = nullptr;
		ActorHandle actor{};
	};
	std::unordered_map<uint64_t, Entry> entries_;
};

class VfxPostEffectTrackAdapter
{
public:
	bool Start(const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken);
	bool Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime);
	void Stop(VfxTrackRuntimeToken token);
	void StopAll();
	[[nodiscard]] uint32_t GetActiveCount() const { return static_cast<uint32_t>(entries_.size()); }

private:
	struct Entry
	{
		std::string effectName;
		bool enabled = false;
	};
	void SetEntryEnabled(Entry& entry, bool enabled);

	std::unordered_map<uint64_t, Entry> entries_;
	std::unordered_map<std::string, uint32_t> effectRefCounts_;
};

class VfxCameraShakeTrackAdapter
{
public:
	bool Start(const VfxTrackStartContext& context, VfxTrackRuntimeToken& outToken);
	bool Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime);
	void Stop(VfxTrackRuntimeToken token);
	void StopAll();
	void BeginFrame();
	void Apply();
	[[nodiscard]] uint32_t GetActiveCount() const { return static_cast<uint32_t>(entries_.size()); }

private:
	struct Entry
	{
		VfxCameraShakeTrackPayload payload{};
		float intensityScale = 1.0f;
		float trackTime = 0.0f;
	};

	void RestorePreviousOffset();
	std::unordered_map<uint64_t, Entry> entries_;
	Vector3 lastTranslationOffset_{};
	Vector3 lastRotationOffsetRadians_{};
	float lastFovOffsetRadians_ = 0.0f;
	bool lastAppliedToDebugCamera_ = false;
	bool hasAppliedOffset_ = false;
};

class VfxTrackAdapterSet
{
public:
	bool Start(
		ActorWorld* world,
		const VfxTrackStartContext& context,
		const VfxRuntimeBudget& budget,
		VfxTrackRuntimeToken& outToken);
	bool Update(VfxTrackRuntimeToken token, const VfxTrackStartContext& context, float trackTime);
	void Stop(VfxTrackRuntimeToken token, VfxCueTrackType type);
	void StopAll();
	void AbandonWorld(const ActorWorld* world);
	void BeginFrame();
	void EndUpdate();

	[[nodiscard]] uint32_t GetActiveParticleCount() const { return particle_.GetActiveCount(); }
	[[nodiscard]] uint32_t GetActiveFluidCount() const { return fluid_.GetActiveCount(); }
	[[nodiscard]] uint32_t GetActiveLightCount() const { return light_.GetActiveCount(); }
	[[nodiscard]] uint32_t GetActivePostEffectCount() const { return postEffect_.GetActiveCount(); }
	[[nodiscard]] uint32_t GetActiveCameraShakeCount() const { return cameraShake_.GetActiveCount(); }

private:
	VfxParticleTrackAdapter particle_{};
	VfxFluidTrackAdapter fluid_{};
	VfxLightTrackAdapter light_{};
	VfxPostEffectTrackAdapter postEffect_{};
	VfxCameraShakeTrackAdapter cameraShake_{};
};

} // namespace Ken4lowEngine
