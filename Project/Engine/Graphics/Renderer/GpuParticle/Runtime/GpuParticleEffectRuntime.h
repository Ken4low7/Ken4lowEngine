#pragma once

#include "GpuParticleEffectModules.h"
#include "GpuParticleEffectSerializer.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
class GpuParticleEffectRuntime
{
public:
	struct PlayHandle
	{
		uint32_t id = 0;
		[[nodiscard]] bool IsValid() const { return id != 0; }
	};

	static GpuParticleEffectRuntime* GetInstance()
	{
		static GpuParticleEffectRuntime instance;
		return &instance;
	}

	bool RegisterEffect(const GpuParticleEffectDesc& effect)
	{
		if (effect.effectName.empty())
		{
			SetStatus(false, "RegisterEffect failed: effectName is empty.");
			return false;
		}

		GpuParticleCompiledEffect compiled = GpuParticleEffectCompiler::Compile(effect);
		if (!ValidateEffectSupport(compiled)) return false;
		RemoveEffectEmitters(effect.effectName);

		ParameterMap previousValues;
		const auto previousIt = effectParameterValues_.find(effect.effectName);
		if (previousIt != effectParameterValues_.end()) previousValues = previousIt->second;

		compiledEffects_[effect.effectName] = std::move(compiled);
		auto& values = effectParameterValues_[effect.effectName];
		values.clear();
		for (const GpuParticleUserParameterDesc& parameter : compiledEffects_[effect.effectName].userParameters)
		{
			const auto valueIt = previousValues.find(parameter.name);
			const float value = valueIt != previousValues.end() ? valueIt->second : parameter.defaultValue;
			values[parameter.name] = std::clamp(value, parameter.minValue, parameter.maxValue);
		}

		nextBurstSlotByEffect_[effect.effectName] = 0;
		SetStatus(true, "Registered effect: " + effect.effectName);
		return true;
	}

	bool LoadEffect(const std::string& filePath)
	{
		GpuParticleEffectDesc effect{};
		if (!GpuParticleEffectSerializer::Load(effect, filePath))
		{
			SetStatus(false, "LoadEffect failed: " + filePath);
			return false;
		}
		const std::string effectName = effect.effectName;
		if (!RegisterEffect(effect)) return false;
		sourcePaths_[effectName] = filePath;
		SetStatus(true, "Loaded effect: " + effectName + " <- " + filePath);
		return true;
	}

	bool ReloadEffect(const std::string& effectName)
	{
		const auto pathIt = sourcePaths_.find(effectName);
		if (pathIt == sourcePaths_.end())
		{
			SetStatus(false, "ReloadEffect failed: source path is not registered. effect=" + effectName);
			return false;
		}
		return LoadEffect(pathIt->second);
	}

	bool Play(const std::string& effectName, const Vector3& worldPosition, float runtimeScale = 1.0f)
	{
		runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);
		const GpuParticleCompiledEffect* effect = FindEffect(effectName);
		if (!effect || !ValidateEffectSupport(*effect)) return false;
		uint32_t& nextSlot = nextBurstSlotByEffect_[effectName];
		const uint32_t burstSlot = nextSlot++ % kBurstEmitterPoolSize;
		bool emittedAny = false;

		for (std::size_t index = 0; index < effect->emitters.size(); ++index)
		{
			const GpuParticleCompiledEmitter& emitterDesc = effect->emitters[index];
			GpuParticleEmitter* emitter = EnsureEmitter(*effect, emitterDesc, index, worldPosition, false, burstSlot, nullptr, runtimeScale);
			if (!emitter)
			{
				SetStatus(false, "Play failed while creating emitter: " + emitterDesc.name);
				return false;
			}
			const uint32_t burstCount = ScaleCount(
				emitterDesc.emission.burstCount,
				EvaluateTargetFactor(*effect, emitterDesc, GpuParticleParameterTarget::BurstCount, nullptr) * runtimeScale);
			if (burstCount > 0) emittedAny |= emitter->RequestEmit(burstCount) > 0;
			emittedAny |= emitter->HasEmissionSchedule();
		}

		SetStatus(emittedAny, emittedAny
			? "Played effect: " + effectName
			: "Play produced no particles. Check burstCount/spawnRate/duration/maxParticles. effect=" + effectName);
		return emittedAny;
	}

	PlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition, float runtimeScale = 1.0f)
	{
		runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);
		const GpuParticleCompiledEffect* effect = FindEffect(effectName);
		if (!effect || !ValidateEffectSupport(*effect)) return {};
		LoopInstance instance{};
		const PlayHandle handle = AllocateHandle();
		instance.handle = handle;
		instance.effectName = effectName;
		instance.worldPosition = worldPosition;
		instance.runtimeScale = runtimeScale;

		for (std::size_t index = 0; index < effect->emitters.size(); ++index)
		{
			const GpuParticleCompiledEmitter& emitterDesc = effect->emitters[index];
			GpuParticleEmitter* emitter = EnsureEmitter(*effect, emitterDesc, index, worldPosition, true, handle.id, &instance.parameterOverrides, runtimeScale);
			if (!emitter)
			{
				RemoveEmitters(instance.emitterNames);
				SetStatus(false, "PlayLoop failed while creating emitter: " + emitterDesc.name);
				return {};
			}
			instance.emitterNames.push_back(BuildEmitterName(effectName, emitterDesc, index, true, handle.id));
			const uint32_t burstCount = ScaleCount(
				emitterDesc.emission.burstCount,
				EvaluateTargetFactor(*effect, emitterDesc, GpuParticleParameterTarget::BurstCount, &instance.parameterOverrides) * runtimeScale);
			if (burstCount > 0) emitter->RequestEmit(burstCount);
		}
		activeLoops_[handle.id] = std::move(instance);
		SetStatus(true, "Loop started: " + effectName);
		return handle;
	}

	bool StopLoop(const std::string& effectName)
	{
		std::vector<uint32_t> handles;
		for (const auto& [handleId, instance] : activeLoops_)
		{
			if (instance.effectName == effectName) handles.push_back(handleId);
		}
		if (handles.empty())
		{
			SetStatus(false, "StopLoop skipped: effect is not looping. effect=" + effectName);
			return false;
		}
		for (const uint32_t handleId : handles) StopLoopInternal(handleId);
		SetStatus(true, "Loop instances stopped: " + effectName);
		return true;
	}

	bool StopLoop(PlayHandle handle)
	{
		if (!handle.IsValid())
		{
			SetStatus(false, "StopLoop skipped: invalid handle.");
			return false;
		}
		const auto instanceIt = activeLoops_.find(handle.id);
		if (instanceIt == activeLoops_.end())
		{
			SetStatus(false, "StopLoop skipped: handle is not active.");
			return false;
		}
		const std::string effectName = instanceIt->second.effectName;
		StopLoopInternal(handle.id);
		SetStatus(true, "Loop stopped: " + effectName);
		return true;
	}

	bool SetLoopPosition(PlayHandle handle, const Vector3& worldPosition)
	{
		auto instanceIt = activeLoops_.find(handle.id);
		if (instanceIt == activeLoops_.end()) return false;
		instanceIt->second.worldPosition = worldPosition;
		return RefreshLoopInstance(instanceIt->second);
	}

	bool SetLoopRuntimeScale(PlayHandle handle, float runtimeScale)
	{
		auto instanceIt = activeLoops_.find(handle.id);
		if (instanceIt == activeLoops_.end() || !std::isfinite(runtimeScale)) return false;
		instanceIt->second.runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);
		return RefreshLoopInstance(instanceIt->second);
	}

	bool SetFloatParameter(const std::string& effectName, const std::string& parameterName, float value)
	{
		const auto effectIt = compiledEffects_.find(effectName);
		if (effectIt == compiledEffects_.end())
		{
			SetStatus(false, "SetFloatParameter failed: effect is not registered. effect=" + effectName);
			return false;
		}
		const GpuParticleUserParameterDesc* parameter = FindParameter(effectIt->second, parameterName);
		if (!parameter)
		{
			SetStatus(false, "SetFloatParameter failed: unknown parameter=" + parameterName);
			return false;
		}
		effectParameterValues_[effectName][parameterName] = std::clamp(value, parameter->minValue, parameter->maxValue);
		for (auto& [handleId, instance] : activeLoops_)
		{
			(void)handleId;
			if (instance.effectName == effectName) RefreshLoopInstance(instance);
		}
		SetStatus(true, "Updated effect parameter: " + effectName + "." + parameterName);
		return true;
	}

	bool SetFloatParameter(PlayHandle handle, const std::string& parameterName, float value)
	{
		auto instanceIt = activeLoops_.find(handle.id);
		if (instanceIt == activeLoops_.end())
		{
			SetStatus(false, "SetFloatParameter failed: loop handle is not active.");
			return false;
		}
		const auto effectIt = compiledEffects_.find(instanceIt->second.effectName);
		if (effectIt == compiledEffects_.end()) return false;
		const GpuParticleUserParameterDesc* parameter = FindParameter(effectIt->second, parameterName);
		if (!parameter)
		{
			SetStatus(false, "SetFloatParameter failed: unknown parameter=" + parameterName);
			return false;
		}
		instanceIt->second.parameterOverrides[parameterName] = std::clamp(value, parameter->minValue, parameter->maxValue);
		const bool refreshed = RefreshLoopInstance(instanceIt->second);
		SetStatus(refreshed, refreshed ? "Updated loop parameter: " + parameterName : "SetFloatParameter failed while refreshing loop emitters.");
		return refreshed;
	}

	[[nodiscard]] bool IsRegistered(const std::string& effectName) const { return compiledEffects_.contains(effectName); }

	[[nodiscard]] bool IsLooping(const std::string& effectName) const
	{
		for (const auto& [handleId, instance] : activeLoops_)
		{
			(void)handleId;
			if (instance.effectName == effectName) return true;
		}
		return false;
	}

	[[nodiscard]] std::size_t GetRegisteredEffectCount() const { return compiledEffects_.size(); }
	[[nodiscard]] std::size_t GetActiveLoopCount() const { return activeLoops_.size(); }
	[[nodiscard]] bool WasLastOperationSuccessful() const { return lastOperationSucceeded_; }
	[[nodiscard]] const std::string& GetLastStatus() const { return lastStatus_; }

private:
	using ParameterMap = std::unordered_map<std::string, float>;

	struct LoopInstance
	{
		PlayHandle handle{};
		std::string effectName;
		Vector3 worldPosition{};
		float runtimeScale = 1.0f;
		ParameterMap parameterOverrides;
		std::vector<std::string> emitterNames;
	};

	static constexpr uint32_t kBurstEmitterPoolSize = 8;
	static constexpr uint32_t kRuntimeMeshBaseStart = 0x60000000u;
	static constexpr uint32_t kRuntimeMeshIdStride = 1024u;
	GpuParticleEffectRuntime() = default;

	[[nodiscard]] const GpuParticleCompiledEffect* FindEffect(const std::string& effectName)
	{
		const auto it = compiledEffects_.find(effectName);
		if (it == compiledEffects_.end())
		{
			SetStatus(false, "Effect is not registered: " + effectName);
			return nullptr;
		}
		return &it->second;
	}

	bool ValidateEffectSupport(const GpuParticleCompiledEffect& effect)
	{
		if (effect.emitters.empty())
		{
			SetStatus(false, "Effect has no emitters: " + effect.name);
			return false;
		}
		std::unordered_set<std::string> parameterNames;
		for (const GpuParticleUserParameterDesc& parameter : effect.userParameters)
		{
			if (parameter.name.empty() || parameter.minValue > parameter.maxValue ||
				!std::isfinite(parameter.defaultValue) || !std::isfinite(parameter.minValue) || !std::isfinite(parameter.maxValue))
			{
				SetStatus(false, "Invalid user parameter in effect: " + effect.name);
				return false;
			}
			if (!parameterNames.insert(parameter.name).second)
			{
				SetStatus(false, "Duplicate user parameter: " + parameter.name);
				return false;
			}
		}

		for (const GpuParticleCompiledEmitter& emitter : effect.emitters)
		{
			if (emitter.emission.maxParticles == 0)
			{
				SetStatus(false, "Emitter maxParticles must be greater than zero: " + emitter.name);
				return false;
			}
			if (static_cast<uint32_t>(emitter.render.renderType) > static_cast<uint32_t>(GpuParticleRenderType::Trail))
			{
				SetStatus(false, "Invalid authored render type: " + emitter.name);
				return false;
			}
			if (emitter.render.renderType == GpuParticleRenderType::Mesh && emitter.render.meshPath.empty())
			{
				SetStatus(false, "Mesh emitter requires meshPath: " + emitter.name);
				return false;
			}
			if ((emitter.render.renderType == GpuParticleRenderType::Ribbon || emitter.render.renderType == GpuParticleRenderType::Trail) && emitter.render.texturePath.empty())
			{
				SetStatus(false, "Ribbon/Trail emitter requires texturePath: " + emitter.name);
				return false;
			}
			if (static_cast<uint32_t>(emitter.spawn.shape) > static_cast<uint32_t>(GpuParticleSpawnShape::Hemisphere))
			{
				SetStatus(false, "Invalid authored spawn shape: " + emitter.name);
				return false;
			}
			if (static_cast<uint32_t>(emitter.render.blendMode) > static_cast<uint32_t>(GpuParticleBlendMode::Multiply))
			{
				SetStatus(false, "Invalid authored blend mode: " + emitter.name);
				return false;
			}
			if (static_cast<uint32_t>(emitter.update.collisionShape) > static_cast<uint32_t>(GpuParticleCollisionShape::Sphere) ||
				static_cast<uint32_t>(emitter.update.collisionResponse) > static_cast<uint32_t>(GpuParticleCollisionResponse::Kill))
			{
				SetStatus(false, "Invalid Phase22 collision configuration: " + emitter.name);
				return false;
			}
			for (const GpuParticleParameterBindingDesc& binding : emitter.parameterBindings)
			{
				if (!FindParameter(effect, binding.parameterName))
				{
					SetStatus(false, "Emitter binding references unknown parameter: " + binding.parameterName);
					return false;
				}
			}
		}
		return true;
	}

	GpuParticleEmitter* EnsureEmitter(
		const GpuParticleCompiledEffect& effect,
		const GpuParticleCompiledEmitter& emitterDesc,
		std::size_t emitterIndex,
		const Vector3& worldPosition,
		bool loopMode,
		uint32_t instanceId,
		const ParameterMap* parameterOverrides,
		float runtimeScale)
	{
		GpuParticleManager* manager = GpuParticleManager::GetInstance();
		const std::string emitterName = BuildEmitterName(effect.name, emitterDesc, emitterIndex, loopMode, instanceId);
		GpuParticleEmitter::EmitterInfo info{};
		if (!CompileEmitterInfo(effect, emitterDesc, loopMode, parameterOverrides, runtimeScale, info)) return nullptr;
		if (GpuParticleEmitter* existing = manager->GetEmitter(emitterName))
		{
			existing->GetInfoMutable() = info;
			if (!loopMode) existing->ResetEmissionSchedule();
			existing->SetPosition(Add(worldPosition, emitterDesc.localPosition));
			return existing;
		}
		GpuParticleEmitter* emitter = manager->CreateRuntimeEmitter(emitterName, info);
		if (!emitter) return nullptr;
		emitter->SetPosition(Add(worldPosition, emitterDesc.localPosition));
		return emitter;
	}

	bool CompileEmitterInfo(
		const GpuParticleCompiledEffect& effect,
		const GpuParticleCompiledEmitter& emitterDesc,
		bool loopMode,
		const ParameterMap* parameterOverrides,
		float runtimeScale,
		GpuParticleEmitter::EmitterInfo& info)
	{
		runtimeScale = std::clamp(runtimeScale, 0.0f, 1.0f);
		const float lifeFactor = EvaluateTargetFactor(effect, emitterDesc, GpuParticleParameterTarget::LifeTime, parameterOverrides);
		const float speedFactor = EvaluateTargetFactor(effect, emitterDesc, GpuParticleParameterTarget::Speed, parameterOverrides);
		const float sizeFactor = EvaluateTargetFactor(effect, emitterDesc, GpuParticleParameterTarget::Size, parameterOverrides);
		const float alphaFactor = EvaluateTargetFactor(effect, emitterDesc, GpuParticleParameterTarget::Alpha, parameterOverrides);
		const float forceFactor = EvaluateTargetFactor(effect, emitterDesc, GpuParticleParameterTarget::Force, parameterOverrides);
		const float spawnRateFactor = EvaluateTargetFactor(effect, emitterDesc, GpuParticleParameterTarget::SpawnRate, parameterOverrides);

		info.radius = (std::max)(emitterDesc.spawn.radius, 0.0f);
		info.drawType = PackGpuParticleDrawType(0u, ToBackendBlendMode(emitterDesc.render.blendMode));
		info.spriteType = GpuParticleType::Default;
		info.lifeScale = 1.0f;
		info.speedScale = 1.0f;
		info.useDescSpawnOverride = true;

		if (emitterDesc.render.renderType == GpuParticleRenderType::Mesh)
		{
			const uint32_t meshId = ResolveMeshId(emitterDesc.render);
			if (meshId == 0u) return false;
			info.kind = GpuParticleKind::Mesh;
			info.textureFilePath = "Mesh:" + std::to_string(meshId);
			info.billboardFlags = BillboardMode::None;
		}
		else if (emitterDesc.render.renderType == GpuParticleRenderType::Ribbon || emitterDesc.render.renderType == GpuParticleRenderType::Trail)
		{
			// Phase23 maps both authoring modes onto the existing velocity-aligned ribbon billboard path.
			info.kind = GpuParticleKind::Ribbon;
			info.ribbonType = GpuRibbonType::Trail;
			info.textureFilePath = emitterDesc.render.texturePath.empty() ? "Effects/white.dds" : emitterDesc.render.texturePath;
			info.billboardFlags = BillboardMode::Ribbon;
		}
		else
		{
			info.kind = GpuParticleKind::Sprite;
			info.textureFilePath = emitterDesc.render.texturePath.empty() ? "Effects/white.dds" : emitterDesc.render.texturePath;
			info.billboardFlags = emitterDesc.render.billboard ? BillboardMode::Camera : BillboardMode::None;
		}

		info.maxParticles = (std::max)(emitterDesc.emission.maxParticles, 1u);
		info.positionRandom = emitterDesc.spawn.positionRandom;
		info.velocity = Scale(emitterDesc.spawn.velocity, speedFactor);
		info.velocityRandom = Scale(emitterDesc.spawn.velocityRandom, speedFactor);
		info.lifeTime = (std::max)(emitterDesc.spawn.lifeTime * lifeFactor, 0.01f);
		info.lifeTimeRandom = (std::max)(emitterDesc.spawn.lifeTimeRandom * lifeFactor, 0.0f);
		info.speed = (std::max)(emitterDesc.spawn.speed * speedFactor, 0.0f);
		info.speedRandom = (std::max)(emitterDesc.spawn.speedRandom * speedFactor, 0.0f);
		info.spawnShape = static_cast<uint32_t>(emitterDesc.spawn.shape);
		info.spawnRadius = (std::max)(emitterDesc.spawn.radius, 0.0f);
		info.spawnBoxSize = emitterDesc.spawn.boxSize;

		info.startSize = Scale(emitterDesc.update.startSize, sizeFactor);
		info.endSize = Scale(emitterDesc.update.endSize, sizeFactor);
		info.sizeRandom = (std::max)(emitterDesc.update.sizeRandom, 0.0f);
		info.useSizeCurve = emitterDesc.update.useSizeCurve;
		info.sizeCurveLut = emitterDesc.update.sizeCurveLut;
		info.startColor = ScaleAlpha(emitterDesc.update.startColor, alphaFactor);
		info.endColor = ScaleAlpha(emitterDesc.update.endColor, alphaFactor);
		info.colorRandom = emitterDesc.update.colorRandom;
		info.alphaFade = emitterDesc.update.alphaFade;
		info.useColorGradient = emitterDesc.update.useColorGradient;
		info.colorGradientLut = emitterDesc.update.colorGradientLut;
		for (Vector4& color : info.colorGradientLut) color = ScaleAlpha(color, alphaFactor);
		info.gravity = Scale(emitterDesc.update.gravity, forceFactor);
		info.damping = (std::max)(emitterDesc.update.damping, 0.0f);
		info.noiseStrength = emitterDesc.update.noiseStrength * forceFactor;
		info.noiseFrequency = (std::max)(emitterDesc.update.noiseFrequency, 0.0f);
		info.vortexAxis = emitterDesc.update.vortexAxis;
		info.vortexStrength = emitterDesc.update.vortexStrength * forceFactor;
		info.attractorPosition = emitterDesc.update.attractorPosition;
		info.attractorStrength = emitterDesc.update.attractorStrength * forceFactor;
		info.attractorRadius = (std::max)(emitterDesc.update.attractorRadius, 0.0f);
		info.startRotation = emitterDesc.update.startRotation;
		info.rotationSpeed = emitterDesc.update.rotationSpeed;
		info.rotationRandom = (std::max)(emitterDesc.update.rotationRandom, 0.0f);
		info.startScale3D = Scale(emitterDesc.update.startScale3D, sizeFactor);
		info.endScale3D = Scale(emitterDesc.update.endScale3D, sizeFactor);
		info.startRotation3D = emitterDesc.update.startRotation3D;
		info.rotationRandom3D = emitterDesc.update.rotationRandom3D;
		info.angularVelocity = emitterDesc.update.angularVelocity;
		info.angularVelocityRandom = emitterDesc.update.angularVelocityRandom;

		// Phase22 update data follows the same compiled-module path as the existing force/curve modules.
		info.collisionShape = static_cast<uint32_t>(emitterDesc.update.collisionShape);
		info.collisionResponse = static_cast<uint32_t>(emitterDesc.update.collisionResponse);
		info.collisionPlaneNormal = emitterDesc.update.collisionPlaneNormal;
		info.collisionPlaneDistance = emitterDesc.update.collisionPlaneDistance;
		info.collisionSphereCenter = emitterDesc.update.collisionSphereCenter;
		info.collisionSphereRadius = emitterDesc.update.collisionSphereRadius;
		info.collisionParticleRadius = emitterDesc.update.collisionParticleRadius;
		info.collisionRestitution = emitterDesc.update.collisionRestitution;
		info.collisionFriction = emitterDesc.update.collisionFriction;
		info.eventMask = emitterDesc.update.eventMask;
		info.subEmitterEventMask = emitterDesc.update.subEmitterEventMask;
		info.subEmitterCount = ScaleCount(emitterDesc.update.subEmitterCount, runtimeScale);
		info.subEmitterLifeTime = emitterDesc.update.subEmitterLifeTime;
		info.subEmitterSpeed = emitterDesc.update.subEmitterSpeed * speedFactor;
		info.subEmitterSpread = emitterDesc.update.subEmitterSpread;
		info.subEmitterInheritVelocity = emitterDesc.update.subEmitterInheritVelocity;
		info.subEmitterStartSize = Scale(emitterDesc.update.subEmitterStartSize, sizeFactor);
		info.subEmitterEndSize = Scale(emitterDesc.update.subEmitterEndSize, sizeFactor);
		info.subEmitterStartColor = ScaleAlpha(emitterDesc.update.subEmitterStartColor, alphaFactor);
		info.subEmitterEndColor = ScaleAlpha(emitterDesc.update.subEmitterEndColor, alphaFactor);
		info.subEmitterAlphaFade = emitterDesc.update.subEmitterAlphaFade;

		info.useSpriteSheet = emitterDesc.render.useSpriteSheet;
		info.spriteSheetRows = static_cast<uint32_t>((std::max)(emitterDesc.render.spriteSheetRows, 1));
		info.spriteSheetColumns = static_cast<uint32_t>((std::max)(emitterDesc.render.spriteSheetColumns, 1));
		info.spriteSheetFrameRate = (std::max)(emitterDesc.render.spriteSheetFrameRate, 0.0f);

		const float effectiveSpawnRate = (std::max)(emitterDesc.emission.spawnRate * spawnRateFactor * runtimeScale, 0.0f);
		info.loopForever = loopMode && emitterDesc.emission.loop;
		info.emissionDuration = info.loopForever ? 0.0f : (std::max)(emitterDesc.emission.duration, 0.0f);
		if (effectiveSpawnRate > 0.0f && (info.loopForever || info.emissionDuration > 0.0f))
		{
			const auto [loopCount, loopFrequency] = BuildLoopSchedule(effectiveSpawnRate);
			info.loopCount = loopCount;
			info.loopFrequency = loopFrequency;
		}
		else
		{
			info.loopCount = 0;
			info.loopFrequency = 0.0f;
		}
		return true;
	}

	uint32_t ResolveMeshId(const GpuParticleRenderModule& render)
	{
		GpuParticleManager* manager = GpuParticleManager::GetInstance();
		uint32_t baseId = 0;
		const auto cachedIt = meshBaseIdsByPath_.find(render.meshPath);
		if (cachedIt != meshBaseIdsByPath_.end()) baseId = cachedIt->second;
		else
		{
			baseId = nextRuntimeMeshBaseId_;
			nextRuntimeMeshBaseId_ += kRuntimeMeshIdStride;
			meshBaseIdsByPath_[render.meshPath] = baseId;
		}
		const uint32_t meshId = baseId + render.meshSubMeshIndex;
		if (!manager->FindMeshAsset(meshId))
		{
			if (!manager->LoadMeshAssetsFromAssimp(baseId, render.meshPath, true) || !manager->FindMeshAsset(meshId))
			{
				SetStatus(false, "Failed to load authored mesh particle asset: " + render.meshPath);
				return 0u;
			}
		}
		if (!render.texturePath.empty()) manager->SetMeshAssetTexturePath(meshId, render.texturePath);
		return meshId;
	}

	bool RefreshLoopInstance(LoopInstance& instance)
	{
		const auto effectIt = compiledEffects_.find(instance.effectName);
		if (effectIt == compiledEffects_.end()) return false;
		const GpuParticleCompiledEffect& effect = effectIt->second;
		if (instance.emitterNames.size() != effect.emitters.size()) return false;
		GpuParticleManager* manager = GpuParticleManager::GetInstance();
		for (std::size_t index = 0; index < effect.emitters.size(); ++index)
		{
			GpuParticleEmitter* emitter = manager->GetEmitter(instance.emitterNames[index]);
			if (!emitter) return false;
			GpuParticleEmitter::EmitterInfo info{};
			if (!CompileEmitterInfo(effect, effect.emitters[index], true, &instance.parameterOverrides, instance.runtimeScale, info)) return false;
			emitter->GetInfoMutable() = info;
			emitter->SetPosition(Add(instance.worldPosition, effect.emitters[index].localPosition));
		}
		return true;
	}

	static std::pair<uint32_t, float> BuildLoopSchedule(float spawnRate)
	{
		if (spawnRate <= 0.0f) return { 0u, 0.0f };
		if (spawnRate < 10.0f) return { 1u, 1.0f / spawnRate };
		constexpr float kHighRateTick = 0.10f;
		const uint32_t count = (std::max)(1u, static_cast<uint32_t>(std::lround(spawnRate * kHighRateTick)));
		return { count, kHighRateTick };
	}

	float EvaluateTargetFactor(const GpuParticleCompiledEffect& effect, const GpuParticleCompiledEmitter& emitter,
		GpuParticleParameterTarget target, const ParameterMap* overrides) const
	{
		float factor = 1.0f;
		for (const GpuParticleParameterBindingDesc& binding : emitter.parameterBindings)
		{
			if (binding.target != target) continue;
			const float value = GetParameterValue(effect, binding.parameterName, overrides);
			factor *= (std::max)(0.0f, binding.bias + binding.scale * value);
		}
		return std::isfinite(factor) ? factor : 0.0f;
	}

	float GetParameterValue(const GpuParticleCompiledEffect& effect, const std::string& parameterName, const ParameterMap* overrides) const
	{
		if (overrides)
		{
			const auto overrideIt = overrides->find(parameterName);
			if (overrideIt != overrides->end()) return overrideIt->second;
		}
		const auto effectValuesIt = effectParameterValues_.find(effect.name);
		if (effectValuesIt != effectParameterValues_.end())
		{
			const auto valueIt = effectValuesIt->second.find(parameterName);
			if (valueIt != effectValuesIt->second.end()) return valueIt->second;
		}
		const GpuParticleUserParameterDesc* parameter = FindParameter(effect, parameterName);
		return parameter ? parameter->defaultValue : 1.0f;
	}

	static const GpuParticleUserParameterDesc* FindParameter(const GpuParticleCompiledEffect& effect, const std::string& name)
	{
		for (const GpuParticleUserParameterDesc& parameter : effect.userParameters)
		{
			if (parameter.name == name) return &parameter;
		}
		return nullptr;
	}

	static BlendMode ToBackendBlendMode(GpuParticleBlendMode mode)
	{
		switch (mode)
		{
		case GpuParticleBlendMode::Alpha: return BlendMode::kBlendModeNormal;
		case GpuParticleBlendMode::Multiply: return BlendMode::kBlendModeMultiply;
		case GpuParticleBlendMode::Additive:
		default: return BlendMode::kBlendModeAdd;
		}
	}

	static uint32_t ScaleCount(uint32_t count, float factor)
	{
		if (count == 0 || factor <= 0.0f || !std::isfinite(factor)) return 0u;
		const double scaled = std::round(static_cast<double>(count) * static_cast<double>(factor));
		const double maxValue = static_cast<double>((std::numeric_limits<uint32_t>::max)());
		return static_cast<uint32_t>(std::clamp(scaled, 1.0, maxValue));
	}

	static Vector2 Scale(const Vector2& value, float factor) { return { value.x * factor, value.y * factor }; }
	static Vector3 Scale(const Vector3& value, float factor) { return { value.x * factor, value.y * factor, value.z * factor }; }
	static Vector4 ScaleAlpha(Vector4 value, float factor)
	{
		value.w = std::clamp(value.w * factor, 0.0f, 1.0f);
		return value;
	}
	static Vector3 Add(const Vector3& lhs, const Vector3& rhs) { return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z }; }

	static std::string BuildEmitterName(const std::string& effectName, const GpuParticleCompiledEmitter& emitterDesc,
		std::size_t emitterIndex, bool loopMode, uint32_t instanceId)
	{
		std::string result = loopMode ? "RuntimeAssetLoop_" : "RuntimeAssetBurst_";
		result += SanitizeName(effectName);
		result += "_" + std::to_string(instanceId);
		result += "_" + std::to_string(emitterIndex) + "_" + SanitizeName(emitterDesc.name);
		return result;
	}

	static std::string SanitizeName(const std::string& value)
	{
		std::string result;
		result.reserve(value.size());
		for (const unsigned char c : value)
		{
			const bool alpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
			const bool digit = c >= '0' && c <= '9';
			result.push_back(alpha || digit || c == '_' || c == '-' ? static_cast<char>(c) : '_');
		}
		return result.empty() ? "Effect" : result;
	}

	void RemoveEffectEmitters(const std::string& effectName)
	{
		GpuParticleManager* manager = GpuParticleManager::GetInstance();
		const auto effectIt = compiledEffects_.find(effectName);
		if (effectIt != compiledEffects_.end())
		{
			for (std::size_t emitterIndex = 0; emitterIndex < effectIt->second.emitters.size(); ++emitterIndex)
			{
				const GpuParticleCompiledEmitter& emitterDesc = effectIt->second.emitters[emitterIndex];
				for (uint32_t slot = 0; slot < kBurstEmitterPoolSize; ++slot)
				{
					manager->RemoveEmitter(BuildEmitterName(effectName, emitterDesc, emitterIndex, false, slot));
				}
			}
		}
		std::vector<uint32_t> loopHandles;
		for (const auto& [handleId, instance] : activeLoops_)
		{
			if (instance.effectName == effectName) loopHandles.push_back(handleId);
		}
		for (const uint32_t handleId : loopHandles) StopLoopInternal(handleId);
	}

	void StopLoopInternal(uint32_t handleId)
	{
		const auto instanceIt = activeLoops_.find(handleId);
		if (instanceIt == activeLoops_.end()) return;
		RemoveEmitters(instanceIt->second.emitterNames);
		activeLoops_.erase(instanceIt);
	}

	static void RemoveEmitters(const std::vector<std::string>& emitterNames)
	{
		GpuParticleManager* manager = GpuParticleManager::GetInstance();
		for (const std::string& emitterName : emitterNames) manager->RemoveEmitter(emitterName);
	}

	PlayHandle AllocateHandle()
	{
		if (nextHandleId_ == 0) nextHandleId_ = 1;
		return PlayHandle{ nextHandleId_++ };
	}

	void SetStatus(bool succeeded, std::string message)
	{
		lastOperationSucceeded_ = succeeded;
		lastStatus_ = std::move(message);
	}

	std::unordered_map<std::string, GpuParticleCompiledEffect> compiledEffects_;
	std::unordered_map<std::string, std::string> sourcePaths_;
	std::unordered_map<std::string, uint32_t> nextBurstSlotByEffect_;
	std::unordered_map<std::string, ParameterMap> effectParameterValues_;
	std::unordered_map<uint32_t, LoopInstance> activeLoops_;
	std::unordered_map<std::string, uint32_t> meshBaseIdsByPath_;
	uint32_t nextRuntimeMeshBaseId_ = kRuntimeMeshBaseStart;
	uint32_t nextHandleId_ = 1;
	bool lastOperationSucceeded_ = true;
	std::string lastStatus_ = "Ready";
};

} // namespace Ken4lowEngine
