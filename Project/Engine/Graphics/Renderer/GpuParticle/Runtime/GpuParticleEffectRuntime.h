#pragma once

#include "GpuParticleEffectModules.h"
#include "GpuParticleEffectSerializer.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Effect Editorで保存したGpuParticleEffectDescをModuleへCompileし、
	/// GameplayからEffect名だけで複数Emitterを再生するRuntime境界です。
	/// </summary>
	class GpuParticleEffectRuntime
	{
	public:
		struct PlayHandle
		{
			uint32_t id = 0;

			[[nodiscard]] bool IsValid() const
			{
				return id != 0;
			}
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

			RemoveEffectEmitters(effect.effectName);
			compiledEffects_[effect.effectName] = GpuParticleEffectCompiler::Compile(effect);
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
			if (!RegisterEffect(effect))
			{
				return false;
			}

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

			const std::string filePath = pathIt->second;
			return LoadEffect(filePath);
		}

		/// <summary>
		/// Effect内の全Emitterを一度だけ発生させます。
		/// 小さなEmitter poolを循環利用し、同一フレームの別位置Spawnで位置を共有しにくくします。
		/// </summary>
		bool Play(const std::string& effectName, const Vector3& worldPosition)
		{
			const GpuParticleCompiledEffect* effect = FindEffect(effectName);
			if (!effect || !ValidateEffectSupport(*effect))
			{
				return false;
			}

			uint32_t& nextSlot = nextBurstSlotByEffect_[effectName];
			const uint32_t burstSlot = nextSlot++ % kBurstEmitterPoolSize;
			bool emittedAny = false;

			for (std::size_t index = 0; index < effect->emitters.size(); ++index)
			{
				const GpuParticleCompiledEmitter& emitterDesc = effect->emitters[index];
				GpuParticleEmitter* emitter = EnsureEmitter(
					effectName, emitterDesc, index, worldPosition, false, burstSlot);
				if (!emitter)
				{
					SetStatus(false, "Play failed while creating emitter: " + emitterDesc.name);
					return false;
				}

				if (emitterDesc.emission.burstCount > 0)
				{
					emittedAny |= emitter->RequestEmit(emitterDesc.emission.burstCount) > 0;
				}
			}

			SetStatus(emittedAny, emittedAny
				? "Played effect: " + effectName
				: "Play produced no particles. Check burstCount/maxParticles. effect=" + effectName);
			return emittedAny;
		}

		/// <summary>
		/// loop=trueのEmitterはspawnRateで継続発生し、loop=falseのEmitterは開始時Burstだけを担当します。
		/// </summary>
		PlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition)
		{
			const GpuParticleCompiledEffect* effect = FindEffect(effectName);
			if (!effect || !ValidateEffectSupport(*effect))
			{
				return {};
			}

			LoopInstance instance{};
			instance.handle = AllocateHandle();
			instance.effectName = effectName;

			for (std::size_t index = 0; index < effect->emitters.size(); ++index)
			{
				const GpuParticleCompiledEmitter& emitterDesc = effect->emitters[index];
				GpuParticleEmitter* emitter = EnsureEmitter(
					effectName, emitterDesc, index, worldPosition, true, instance.handle.id);
				if (!emitter)
				{
					RemoveEmitters(instance.emitterNames);
					SetStatus(false, "PlayLoop failed while creating emitter: " + emitterDesc.name);
					return {};
				}

				instance.emitterNames.push_back(
					BuildEmitterName(effectName, emitterDesc, index, true, instance.handle.id));
				if (emitterDesc.emission.burstCount > 0)
				{
					emitter->RequestEmit(emitterDesc.emission.burstCount);
				}
			}

			activeLoops_[instance.handle.id] = instance;
			SetStatus(true, "Loop started: " + effectName);
			return instance.handle;
		}

		bool StopLoop(const std::string& effectName)
		{
			std::vector<uint32_t> handles;
			for (const auto& [handleId, instance] : activeLoops_)
			{
				if (instance.effectName == effectName)
				{
					handles.push_back(handleId);
				}
			}

			if (handles.empty())
			{
				SetStatus(false, "StopLoop skipped: effect is not looping. effect=" + effectName);
				return false;
			}

			for (const uint32_t handleId : handles)
			{
				StopLoopInternal(handleId);
			}
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
			const auto instanceIt = activeLoops_.find(handle.id);
			if (instanceIt == activeLoops_.end())
			{
				return false;
			}

			const auto effectIt = compiledEffects_.find(instanceIt->second.effectName);
			if (effectIt == compiledEffects_.end())
			{
				return false;
			}

			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			const GpuParticleCompiledEffect& effect = effectIt->second;
			for (std::size_t index = 0; index < effect.emitters.size(); ++index)
			{
				const GpuParticleCompiledEmitter& emitterDesc = effect.emitters[index];
				const std::string emitterName = BuildEmitterName(
					effect.name, emitterDesc, index, true, handle.id);
				if (GpuParticleEmitter* emitter = manager->GetEmitter(emitterName))
				{
					emitter->SetPosition(Add(worldPosition, emitterDesc.localPosition));
				}
			}
			return true;
		}

		[[nodiscard]] bool IsRegistered(const std::string& effectName) const
		{
			return compiledEffects_.contains(effectName);
		}

		[[nodiscard]] bool IsLooping(const std::string& effectName) const
		{
			for (const auto& [handleId, instance] : activeLoops_)
			{
				(void)handleId;
				if (instance.effectName == effectName)
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] std::size_t GetRegisteredEffectCount() const
		{
			return compiledEffects_.size();
		}

		[[nodiscard]] std::size_t GetActiveLoopCount() const
		{
			return activeLoops_.size();
		}

		[[nodiscard]] bool WasLastOperationSuccessful() const
		{
			return lastOperationSucceeded_;
		}

		[[nodiscard]] const std::string& GetLastStatus() const
		{
			return lastStatus_;
		}

	private:
		struct LoopInstance
		{
			PlayHandle handle{};
			std::string effectName;
			std::vector<std::string> emitterNames;
		};

		static constexpr uint32_t kBurstEmitterPoolSize = 8;

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

			for (const GpuParticleCompiledEmitter& emitter : effect.emitters)
			{
				if (emitter.render.renderType != GpuParticleRenderType::Sprite)
				{
					SetStatus(false, "Unsupported authored renderer (currently Sprite only): " + emitter.name);
					return false;
				}
				if (emitter.render.blendMode != GpuParticleBlendMode::Additive)
				{
					// 既存GpuParticleSpritePipelineが加算PSO固定なので、見た目を偽装せずAdditiveだけを許可する。
					SetStatus(false, "Unsupported authored blend mode (current backend is Additive): " + emitter.name);
					return false;
				}
				if (!IsSpawnShapeSupported(emitter.spawn.shape))
				{
					SetStatus(false, "Unsupported authored spawn shape (current backend supports Point/Sphere/Box): " + emitter.name);
					return false;
				}
			}
			return true;
		}

		GpuParticleEmitter* EnsureEmitter(
			const std::string& effectName,
			const GpuParticleCompiledEmitter& emitterDesc,
			std::size_t emitterIndex,
			const Vector3& worldPosition,
			bool loopMode,
			uint32_t instanceId)
		{
			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			const std::string emitterName = BuildEmitterName(
				effectName, emitterDesc, emitterIndex, loopMode, instanceId);
			if (GpuParticleEmitter* existing = manager->GetEmitter(emitterName))
			{
				existing->SetPosition(Add(worldPosition, emitterDesc.localPosition));
				return existing;
			}

			GpuParticleEmitter::EmitterInfo info = CompileEmitterInfo(emitterDesc, loopMode);
			GpuParticleEmitter* emitter = manager->CreateRuntimeEmitter(emitterName, info);
			if (!emitter)
			{
				return nullptr;
			}

			emitter->SetPosition(Add(worldPosition, emitterDesc.localPosition));
			return emitter;
		}

		static GpuParticleEmitter::EmitterInfo CompileEmitterInfo(
			const GpuParticleCompiledEmitter& emitterDesc,
			bool loopMode)
		{
			GpuParticleEmitter::EmitterInfo info{};
			info.textureFilePath = emitterDesc.render.texturePath.empty()
				? "Effects/white.dds" : emitterDesc.render.texturePath;
			info.radius = (std::max)(emitterDesc.spawn.radius, 0.0f);
			info.drawType = 0;
			info.kind = GpuParticleKind::Sprite;
			info.spriteType = GpuParticleType::Default;
			info.billboardFlags = emitterDesc.render.billboard ? BillboardMode::Camera : BillboardMode::None;
			info.lifeScale = 1.0f;
			info.speedScale = 1.0f;
			info.useDescSpawnOverride = true;

			// Moduleごとの責務から既存EmitterInfoへ集約し、GPUバックエンドは変更せずAuthoring構造だけを分離する。
			info.maxParticles = (std::max)(emitterDesc.emission.maxParticles, 1u);
			info.positionRandom = emitterDesc.spawn.positionRandom;
			info.velocity = emitterDesc.spawn.velocity;
			info.velocityRandom = emitterDesc.spawn.velocityRandom;
			info.lifeTime = (std::max)(emitterDesc.spawn.lifeTime, 0.01f);
			info.lifeTimeRandom = (std::max)(emitterDesc.spawn.lifeTimeRandom, 0.0f);
			info.speed = (std::max)(emitterDesc.spawn.speed, 0.0f);
			info.speedRandom = (std::max)(emitterDesc.spawn.speedRandom, 0.0f);
			info.spawnShape = static_cast<uint32_t>(emitterDesc.spawn.shape);
			info.spawnRadius = (std::max)(emitterDesc.spawn.radius, 0.0f);
			info.spawnBoxSize = emitterDesc.spawn.boxSize;

			info.startSize = emitterDesc.update.startSize;
			info.endSize = emitterDesc.update.endSize;
			info.sizeRandom = (std::max)(emitterDesc.update.sizeRandom, 0.0f);
			info.startColor = emitterDesc.update.startColor;
			info.endColor = emitterDesc.update.endColor;
			info.colorRandom = emitterDesc.update.colorRandom;
			info.alphaFade = emitterDesc.update.alphaFade;
			info.gravity = emitterDesc.update.gravity;
			info.damping = (std::max)(emitterDesc.update.damping, 0.0f);
			info.startRotation = emitterDesc.update.startRotation;
			info.rotationSpeed = emitterDesc.update.rotationSpeed;
			info.rotationRandom = (std::max)(emitterDesc.update.rotationRandom, 0.0f);
			info.startScale3D = emitterDesc.update.startScale3D;
			info.endScale3D = emitterDesc.update.endScale3D;

			info.useSpriteSheet = emitterDesc.render.useSpriteSheet;
			info.spriteSheetRows = static_cast<uint32_t>((std::max)(emitterDesc.render.spriteSheetRows, 1));
			info.spriteSheetColumns = static_cast<uint32_t>((std::max)(emitterDesc.render.spriteSheetColumns, 1));
			info.spriteSheetFrameRate = (std::max)(emitterDesc.render.spriteSheetFrameRate, 0.0f);

			if (loopMode && emitterDesc.emission.loop && emitterDesc.emission.spawnRate > 0.0f)
			{
				const auto [loopCount, loopFrequency] = BuildLoopSchedule(emitterDesc.emission.spawnRate);
				info.loopCount = loopCount;
				info.loopFrequency = loopFrequency;
			}
			else
			{
				info.loopCount = 0;
				info.loopFrequency = 0.0f;
			}
			return info;
		}

		static std::pair<uint32_t, float> BuildLoopSchedule(float spawnRate)
		{
			if (spawnRate <= 0.0f)
			{
				return { 0u, 0.0f };
			}

			if (spawnRate < 10.0f)
			{
				return { 1u, 1.0f / spawnRate };
			}

			constexpr float kHighRateTick = 0.10f;
			const uint32_t count = (std::max)(
				1u, static_cast<uint32_t>(std::lround(spawnRate * kHighRateTick)));
			return { count, kHighRateTick };
		}

		static bool IsSpawnShapeSupported(GpuParticleSpawnShape shape)
		{
			return shape == GpuParticleSpawnShape::Point ||
				shape == GpuParticleSpawnShape::Sphere ||
				shape == GpuParticleSpawnShape::Box;
		}

		static Vector3 Add(const Vector3& lhs, const Vector3& rhs)
		{
			return { lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z };
		}

		static std::string BuildEmitterName(
			const std::string& effectName,
			const GpuParticleCompiledEmitter& emitterDesc,
			std::size_t emitterIndex,
			bool loopMode,
			uint32_t instanceId)
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
						manager->RemoveEmitter(BuildEmitterName(
							effectName, emitterDesc, emitterIndex, false, slot));
					}
				}
			}

			std::vector<uint32_t> loopHandles;
			for (const auto& [handleId, instance] : activeLoops_)
			{
				if (instance.effectName == effectName)
				{
					loopHandles.push_back(handleId);
				}
			}
			for (const uint32_t handleId : loopHandles)
			{
				StopLoopInternal(handleId);
			}
		}

		void StopLoopInternal(uint32_t handleId)
		{
			const auto instanceIt = activeLoops_.find(handleId);
			if (instanceIt == activeLoops_.end())
			{
				return;
			}

			RemoveEmitters(instanceIt->second.emitterNames);
			activeLoops_.erase(instanceIt);
		}

		static void RemoveEmitters(const std::vector<std::string>& emitterNames)
		{
			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			for (const std::string& emitterName : emitterNames)
			{
				manager->RemoveEmitter(emitterName);
			}
		}

		PlayHandle AllocateHandle()
		{
			if (nextHandleId_ == 0)
			{
				nextHandleId_ = 1;
			}
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
		std::unordered_map<uint32_t, LoopInstance> activeLoops_;
		uint32_t nextHandleId_ = 1;
		bool lastOperationSucceeded_ = true;
		std::string lastStatus_ = "Ready";
	};

} // namespace Ken4lowEngine
