#include "VfxCueRuntime.h"

#include "../Asset/VfxCueSerializer.h"
#include "Engine/Scene/Actor/Core/ActorWorld.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Ken4lowEngine
{
namespace
{
	constexpr float kTimelineEpsilon = 0.00001f;

	const VfxCueUserParameterDesc* FindParameter(const VfxCueProgram& program, const std::string& name)
	{
		for (const VfxCueUserParameterDesc& parameter : program.userParameters)
		{
			if (parameter.name == name) return &parameter;
		}
		return nullptr;
	}
}

VfxCueRuntime* VfxCueRuntime::GetInstance()
{
	static VfxCueRuntime instance;
	return &instance;
}

bool VfxCueRuntime::Initialize()
{
	if (initialized_) return true;
	initialized_ = true;
	stats_ = {};
	SetStatus(true, "VFX Runtime initialized.");
	return true;
}

void VfxCueRuntime::Finalize()
{
	StopAll();
	adapters_.StopAll();
	cues_.clear();
	instances_.clear();
	stats_ = {};
	activeWorld_ = nullptr;
	nextHandleValue_ = 1;
	nextTrackId_ = 1;
	initialized_ = false;
}

void VfxCueRuntime::BeginFrame()
{
	if (!initialized_) return;
	adapters_.BeginFrame();
}

void VfxCueRuntime::Update(float deltaTime, ActorWorld* world)
{
	if (!initialized_ && !Initialize()) return;
	SwitchActiveWorld(world);
	stats_.trackStartsThisFrame = 0;

	if (!std::isfinite(deltaTime) || deltaTime < 0.0f)
	{
		deltaTime = 0.0f;
	}
	// Pause復帰やDebugger停止で巨大な1Frameを流し込み、Cue全Trackを一度に消費しない。
	deltaTime = (std::min)(deltaTime, 0.25f);

	for (auto it = instances_.begin(); it != instances_.end();)
	{
		if (!AdvanceInstance(it->second, deltaTime, world))
		{
			StopInstanceTracks(it->second);
			it = instances_.erase(it);
			++stats_.completedInstances;
		}
		else
		{
			++it;
		}
	}

	adapters_.EndUpdate();
	RefreshStats();
}

void VfxCueRuntime::SwitchActiveWorld(ActorWorld* world)
{
	if (activeWorld_ == world) return;

	if (activeWorld_ != nullptr)
	{
		// SceneManagerが旧Worldを先に破棄していても、Fluid/Light Adapterはpointer値比較だけで参照を捨てる。
		adapters_.AbandonWorld(activeWorld_);
		for (auto& [id, instance] : instances_)
		{
			(void)id;
			StopInstanceTracks(instance); // World依存tokenは既に消えているため、残るParticle/Post/Cameraだけ通常停止する。
		}
		instances_.clear();
	}
	activeWorld_ = world;
	RefreshStats();
}

bool VfxCueRuntime::RegisterCue(const VfxCueDesc& desc, const std::string& sourcePath)
{
	VfxCueProgram program{};
	std::string error;
	if (!VfxCueCompiler::Compile(desc, program, &error))
	{
		SetStatus(false, "VFX RegisterCue failed: " + error);
		return false;
	}

	RegisteredCue registered{};
	registered.desc = desc;
	registered.program = std::move(program);
	registered.sourcePath = sourcePath;
	cues_[desc.cueName] = std::move(registered);
	RefreshStats();
	SetStatus(true, "Registered VFX cue: " + desc.cueName);
	return true;
}

bool VfxCueRuntime::LoadCue(const std::string& filePath)
{
	VfxCueDesc desc{};
	if (!VfxCueSerializer::Load(desc, filePath))
	{
		SetStatus(false, "VFX LoadCue failed: " + filePath);
		return false;
	}
	return RegisterCue(desc, filePath);
}

bool VfxCueRuntime::ReloadCue(const std::string& cueName)
{
	const auto it = cues_.find(cueName);
	if (it == cues_.end() || it->second.sourcePath.empty())
	{
		SetStatus(false, "VFX ReloadCue failed: source path is not registered. cue=" + cueName);
		return false;
	}
	const std::string path = it->second.sourcePath;
	if (!LoadCue(path)) return false;
	++stats_.hotReloadCount;
	SetStatus(true, "Hot reloaded VFX cue: " + cueName);
	return true;
}

bool VfxCueRuntime::UnregisterCue(const std::string& cueName)
{
	StopAll(cueName);
	if (cues_.erase(cueName) == 0)
	{
		SetStatus(false, "VFX cue is not registered: " + cueName);
		return false;
	}
	RefreshStats();
	SetStatus(true, "Unregistered VFX cue: " + cueName);
	return true;
}

VfxCueHandle VfxCueRuntime::Play(const std::string& cueName, const Vector3& worldPosition)
{
	++stats_.totalPlayRequests;
	const auto cueIt = cues_.find(cueName);
	if (cueIt == cues_.end())
	{
		SetStatus(false, "VFX Play failed: cue is not registered. cue=" + cueName);
		return {};
	}
	if (instances_.size() >= budget_.maxActiveInstances)
	{
		++stats_.budgetRejectedInstances;
		SetStatus(false, "VFX Play rejected by active-instance budget.");
		return {};
	}

	Instance instance{};
	instance.handle = AllocateHandle();
	instance.cueName = cueName;
	instance.program = cueIt->second.program; // Hot Reloadで再生中Programを差し替えず、Instance開始時の定義を固定する。
	instance.worldPosition = worldPosition;
	for (const VfxCueUserParameterDesc& parameter : instance.program.userParameters)
	{
		instance.parameters[parameter.name] = parameter.defaultValue;
	}

	const VfxCueHandle handle = instance.handle;
	instances_.emplace(handle.value, std::move(instance));
	RefreshStats();
	SetStatus(true, "Played VFX cue: " + cueName);
	return handle;
}

bool VfxCueRuntime::Stop(VfxCueHandle handle)
{
	++stats_.totalStopRequests;
	const auto it = instances_.find(handle.value);
	if (!handle.IsValid() || it == instances_.end())
	{
		SetStatus(false, "VFX Stop skipped: handle is not active.");
		return false;
	}
	StopInstanceTracks(it->second);
	const std::string cueName = it->second.cueName;
	instances_.erase(it);
	RefreshStats();
	SetStatus(true, "Stopped VFX cue: " + cueName);
	return true;
}

uint32_t VfxCueRuntime::StopAll(const std::string& cueName)
{
	uint32_t count = 0;
	for (auto it = instances_.begin(); it != instances_.end();)
	{
		if (it->second.cueName != cueName)
		{
			++it;
			continue;
		}
		StopInstanceTracks(it->second);
		it = instances_.erase(it);
		++count;
	}
	stats_.totalStopRequests += count;
	RefreshStats();
	return count;
}

void VfxCueRuntime::StopAll()
{
	for (auto& [id, instance] : instances_)
	{
		(void)id;
		StopInstanceTracks(instance);
	}
	stats_.totalStopRequests += instances_.size();
	instances_.clear();
	adapters_.StopAll();
	RefreshStats();
}

bool VfxCueRuntime::SetWorldPosition(VfxCueHandle handle, const Vector3& worldPosition)
{
	const auto it = instances_.find(handle.value);
	if (it == instances_.end()) return false;
	it->second.worldPosition = worldPosition;
	return true;
}

bool VfxCueRuntime::SetFloatParameter(VfxCueHandle handle, const std::string& parameterName, float value)
{
	const auto it = instances_.find(handle.value);
	if (it == instances_.end())
	{
		SetStatus(false, "VFX SetFloatParameter failed: handle is not active.");
		return false;
	}
	const VfxCueUserParameterDesc* parameter = FindParameter(it->second.program, parameterName);
	if (parameter == nullptr || !std::isfinite(value))
	{
		SetStatus(false, "VFX SetFloatParameter failed: unknown/invalid parameter=" + parameterName);
		return false;
	}
	it->second.parameters[parameterName] = std::clamp(value, parameter->minValue, parameter->maxValue);
	SetStatus(true, "Updated VFX parameter: " + parameterName);
	return true;
}

bool VfxCueRuntime::SetRuntimeScale(VfxCueHandle handle, float runtimeScale)
{
	const auto it = instances_.find(handle.value);
	if (it == instances_.end() || !std::isfinite(runtimeScale)) return false;
	it->second.runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);
	return true;
}

uint32_t VfxCueRuntime::RunStressBurst(
	const std::string& cueName,
	uint32_t count,
	const Vector3& center,
	float spacing)
{
	if (!IsCueRegistered(cueName) || count == 0) return 0;
	count = (std::min)(count, budget_.maxActiveInstances);
	spacing = (std::max)(0.1f, spacing);
	const uint32_t side = (std::max)(1u, static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<float>(count)))));
	uint32_t played = 0;
	for (uint32_t index = 0; index < count; ++index)
	{
		const uint32_t x = index % side;
		const uint32_t z = index / side;
		const float ox = (static_cast<float>(x) - static_cast<float>(side - 1) * 0.5f) * spacing;
		const float oz = (static_cast<float>(z) - static_cast<float>(side - 1) * 0.5f) * spacing;
		if (Play(cueName, { center.x + ox, center.y, center.z + oz }).IsValid()) ++played;
	}
	stats_.stressPlayCount += played;
	return played;
}

bool VfxCueRuntime::IsCueRegistered(const std::string& cueName) const
{
	return cues_.contains(cueName);
}

bool VfxCueRuntime::IsHandleActive(VfxCueHandle handle) const
{
	return handle.IsValid() && instances_.contains(handle.value);
}

const VfxCueDesc* VfxCueRuntime::GetRegisteredCueDesc(const std::string& cueName) const
{
	const auto it = cues_.find(cueName);
	return it == cues_.end() ? nullptr : &it->second.desc;
}

const VfxCueProgram* VfxCueRuntime::GetRegisteredProgram(const std::string& cueName) const
{
	const auto it = cues_.find(cueName);
	return it == cues_.end() ? nullptr : &it->second.program;
}

const std::string* VfxCueRuntime::GetSourcePath(const std::string& cueName) const
{
	const auto it = cues_.find(cueName);
	return it == cues_.end() ? nullptr : &it->second.sourcePath;
}

bool VfxCueRuntime::AdvanceInstance(Instance& instance, float deltaTime, ActorWorld* world)
{
	if (instance.justStarted)
	{
		instance.justStarted = false;
		StartDueTracks(instance, world);
		UpdateActiveTracks(instance);
		StopExpiredTracks(instance);
	}

	float remaining = deltaTime;
	uint32_t loopGuard = 0;
	do
	{
		const float duration = instance.program.duration;
		const float target = duration > 0.0f
			? (std::min)(instance.elapsed + remaining, duration)
			: instance.elapsed + remaining;
		const float advanced = target - instance.elapsed;
		instance.elapsed = target;
		remaining = (std::max)(0.0f, remaining - advanced);

		StartDueTracks(instance, world);
		UpdateActiveTracks(instance);
		StopExpiredTracks(instance);

		if (duration <= 0.0f || instance.elapsed + kTimelineEpsilon < duration)
		{
			break;
		}

		StopInstanceTracks(instance);
		if (!instance.program.loop)
		{
			return false;
		}

		instance.elapsed = 0.0f;
		instance.nextInstructionIndex = 0;
		StartDueTracks(instance, world);
		++loopGuard;
	} while (remaining > 0.0f && loopGuard < 8u);

	if (!instance.program.loop &&
		instance.nextInstructionIndex >= instance.program.instructions.size() &&
		instance.activeTracks.empty() &&
		instance.elapsed + kTimelineEpsilon >= instance.program.duration)
	{
		return false;
	}
	return true;
}

bool VfxCueRuntime::StartDueTracks(Instance& instance, ActorWorld* world)
{
	while (instance.nextInstructionIndex < instance.program.instructions.size())
	{
		const uint32_t instructionIndex = instance.nextInstructionIndex;
		const VfxCueInstruction& instruction = instance.program.instructions[instructionIndex];
		if (instruction.startTime > instance.elapsed + kTimelineEpsilon)
		{
			break;
		}
		if (stats_.trackStartsThisFrame >= budget_.maxTrackStartsPerFrame ||
			stats_.activeTrackCount + stats_.trackStartsThisFrame >= budget_.maxActiveTracks)
		{
			++stats_.budgetDelayedTrackStarts;
			return false; // nextInstructionIndexを進めず、次Frameへ遅延する。
		}

		++instance.nextInstructionIndex;
		++stats_.trackStartsThisFrame;
		if (!StartTrack(instance, instructionIndex, world))
		{
			++stats_.adapterFailures;
		}
	}
	return true;
}

bool VfxCueRuntime::StartTrack(Instance& instance, uint32_t instructionIndex, ActorWorld* world)
{
	const VfxCueInstruction& instruction = instance.program.instructions[instructionIndex];
	const uint64_t runtimeTrackId = AllocateTrackId();
	VfxTrackStartContext context = BuildTrackContext(instance, instructionIndex, runtimeTrackId);
	VfxTrackRuntimeToken token{};
	if (!adapters_.Start(world, context, budget_, token))
	{
		return false;
	}
	++stats_.totalTrackStarts;

	if (instruction.endTime <= instruction.startTime + kTimelineEpsilon)
	{
		adapters_.Stop(token, instruction.type);
		++stats_.totalTrackStops;
		return true;
	}

	instance.activeTracks.push_back({ instructionIndex, runtimeTrackId, token });
	return true;
}

void VfxCueRuntime::UpdateActiveTracks(Instance& instance)
{
	for (const ActiveTrack& active : instance.activeTracks)
	{
		const VfxCueInstruction& instruction = instance.program.instructions[active.instructionIndex];
		VfxTrackStartContext context = BuildTrackContext(instance, active.instructionIndex, active.runtimeTrackId);
		const float trackTime = (std::max)(0.0f, instance.elapsed - instruction.startTime);
		if (!adapters_.Update(active.token, context, trackTime))
		{
			++stats_.adapterFailures;
		}
	}
}

void VfxCueRuntime::StopExpiredTracks(Instance& instance)
{
	for (auto it = instance.activeTracks.begin(); it != instance.activeTracks.end();)
	{
		const VfxCueInstruction& instruction = instance.program.instructions[it->instructionIndex];
		if (instance.elapsed + kTimelineEpsilon < instruction.endTime)
		{
			++it;
			continue;
		}
		adapters_.Stop(it->token, instruction.type);
		++stats_.totalTrackStops;
		it = instance.activeTracks.erase(it);
	}
}

void VfxCueRuntime::StopInstanceTracks(Instance& instance)
{
	for (const ActiveTrack& active : instance.activeTracks)
	{
		const VfxCueInstruction& instruction = instance.program.instructions[active.instructionIndex];
		adapters_.Stop(active.token, instruction.type);
		++stats_.totalTrackStops;
	}
	instance.activeTracks.clear();
}

VfxTrackStartContext VfxCueRuntime::BuildTrackContext(
	const Instance& instance,
	uint32_t instructionIndex,
	uint64_t runtimeTrackId) const
{
	const VfxCueInstruction& instruction = instance.program.instructions[instructionIndex];
	VfxTrackStartContext context{};
	context.cueHandle = instance.handle;
	context.runtimeTrackId = runtimeTrackId;
	context.type = instruction.type;
	context.worldPosition = instance.worldPosition;
	context.localOffset = instruction.localOffset;
	context.payload = instruction.payload;
	context.parameters = ResolveTrackParameters(instance, instruction);
	return context;
}

VfxResolvedTrackParameters VfxCueRuntime::ResolveTrackParameters(
	const Instance& instance,
	const VfxCueInstruction& instruction) const
{
	VfxResolvedTrackParameters resolved{};
	for (const VfxCueTrackBindingDesc& binding : instruction.bindings)
	{
		const auto valueIt = instance.parameters.find(binding.parameterName);
		if (valueIt == instance.parameters.end()) continue;
		const float value = valueIt->second * binding.scale + binding.bias;
		switch (binding.target)
		{
		case VfxCueBindingTarget::IntensityScale:
			resolved.intensityScale *= (std::max)(0.0f, value);
			break;
		case VfxCueBindingTarget::RadiusScale:
			resolved.radiusScale *= (std::max)(0.001f, value);
			break;
		case VfxCueBindingTarget::ParticleFloat:
			if (!binding.targetName.empty()) resolved.particleFloatOverrides[binding.targetName] = value;
			break;
		default:
			break;
		}
	}
	// Phase27 applies graph LOD to existing Fluid/Light/PostEffect adapters without duplicating subsystem backends.
	resolved.intensityScale *= instance.runtimeScale;
	return resolved;
}

void VfxCueRuntime::RefreshStats()
{
	stats_.registeredCueCount = static_cast<uint32_t>(cues_.size());
	stats_.activeInstanceCount = static_cast<uint32_t>(instances_.size());
	stats_.activeTrackCount = 0;
	stats_.activeParticleTrackCount = 0;
	stats_.activeFluid2DTrackCount = 0;
	stats_.activeVolumetricTrackCount = 0;
	stats_.activeLightTrackCount = 0;
	stats_.activePostEffectTrackCount = 0;
	stats_.activeCameraShakeTrackCount = 0;
	for (const auto& [id, instance] : instances_)
	{
		(void)id;
		stats_.activeTrackCount += static_cast<uint32_t>(instance.activeTracks.size());
		for (const ActiveTrack& active : instance.activeTracks)
		{
			switch (instance.program.instructions[active.instructionIndex].type)
			{
			case VfxCueTrackType::Particle: ++stats_.activeParticleTrackCount; break;
			case VfxCueTrackType::Fluid2D: ++stats_.activeFluid2DTrackCount; break;
			case VfxCueTrackType::VolumetricFluid: ++stats_.activeVolumetricTrackCount; break;
			case VfxCueTrackType::Light: ++stats_.activeLightTrackCount; break;
			case VfxCueTrackType::PostEffect: ++stats_.activePostEffectTrackCount; break;
			case VfxCueTrackType::CameraShake: ++stats_.activeCameraShakeTrackCount; break;
			default: break;
			}
		}
	}
	stats_.peakActiveInstanceCount = (std::max)(stats_.peakActiveInstanceCount, stats_.activeInstanceCount);
	stats_.peakActiveTrackCount = (std::max)(stats_.peakActiveTrackCount, stats_.activeTrackCount);
}

void VfxCueRuntime::SetStatus(bool success, std::string message)
{
	stats_.lastOperationSucceeded = success;
	stats_.lastStatus = std::move(message);
}

VfxCueHandle VfxCueRuntime::AllocateHandle()
{
	if (nextHandleValue_ == 0) nextHandleValue_ = 1;
	return { nextHandleValue_++ };
}

uint64_t VfxCueRuntime::AllocateTrackId()
{
	if (nextTrackId_ == 0) nextTrackId_ = 1;
	return nextTrackId_++;
}

} // namespace Ken4lowEngine
