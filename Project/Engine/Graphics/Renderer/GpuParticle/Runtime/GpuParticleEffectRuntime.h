#pragma once

#include "GpuParticleEffectDesc.h"
#include "GpuParticleEffectSerializer.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>
	/// Effect Editorで保存したGpuParticleEffectDescを、そのまま実ゲームで再生するためのRuntime境界です。
	/// Gameplay側はEmitterやGpuParticleTypeを直接触らず、Effect名と位置だけを指定します。
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

		/// <summary>
		/// Editor等で組み立てたEffect設定をRuntimeへ登録します。
		/// 同名Effectを再登録した場合は既存Runtime Emitterを破棄し、新設定を次回再生から使用します。
		/// </summary>
		bool RegisterEffect(const GpuParticleEffectDesc& effect)
		{
			if (effect.effectName.empty())
			{
				SetStatus(false, "RegisterEffect failed: effectName is empty.");
				return false;
			}

			RemoveCachedEmitters(effect.effectName);
			effects_[effect.effectName] = effect;
			SetStatus(true, "Registered effect: " + effect.effectName);
			return true;
		}

		/// <summary>
		/// GpuParticleEffectSerializerのJSONを読み込み、effectNameをキーにRuntimeへ登録します。
		/// </summary>
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

		/// <summary>
		/// LoadEffectで登録したJSONを再読み込みします。Particle Editorの保存後に即座に再確認する用途です。
		/// </summary>
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
		/// Effect内の全Sprite Emitterを一度だけ発生させます。
		/// 同一EffectのEmitterは再利用するため、短時間に連続再生してもEmitterオブジェクトを増やし続けません。
		/// </summary>
		bool Play(const std::string& effectName, const Vector3& worldPosition)
		{
			const GpuParticleEffectDesc* effect = FindEffect(effectName);
			if (!effect)
			{
				return false;
			}

			bool emittedAny = false;
			for (std::size_t index = 0; index < effect->emitters.size(); ++index)
			{
				const GpuParticleEmitterDesc& desc = effect->emitters[index];
				if (desc.renderType != GpuParticleRenderType::Sprite)
				{
					continue;
				}

				GpuParticleEmitter* emitter = EnsureEmitter(effectName, desc, index, worldPosition, false);
				if (!emitter)
				{
					continue;
				}

				const uint32_t count = desc.burstCount;
				if (count > 0)
				{
					emittedAny |= emitter->RequestEmit(count) > 0;
				}
			}

			SetStatus(emittedAny, emittedAny
				? "Played effect: " + effectName
				: "Play produced no particles. Check burstCount / renderType. effect=" + effectName);
			return emittedAny;
		}

		/// <summary>
		/// Effectを継続再生します。desc.loop=trueのEmitterだけspawnRateに従って定期発生し、
		/// loop=falseのEmitterは開始時burstだけを担当できるため、Flash + Smokeのような複合演出を1Effectにできます。
		/// </summary>
		PlayHandle PlayLoop(const std::string& effectName, const Vector3& worldPosition)
		{
			const GpuParticleEffectDesc* effect = FindEffect(effectName);
			if (!effect)
			{
				return {};
			}

			auto activeIt = activeLoops_.find(effectName);
			if (activeIt != activeLoops_.end())
			{
				SetLoopPosition(activeIt->second.handle, worldPosition);
				SetStatus(true, "Loop position updated: " + effectName);
				return activeIt->second.handle;
			}

			LoopInstance instance{};
			instance.handle = AllocateHandle();
			instance.effectName = effectName;

			bool createdAny = false;
			for (std::size_t index = 0; index < effect->emitters.size(); ++index)
			{
				const GpuParticleEmitterDesc& desc = effect->emitters[index];
				if (desc.renderType != GpuParticleRenderType::Sprite)
				{
					continue;
				}

				GpuParticleEmitter* emitter = EnsureEmitter(effectName, desc, index, worldPosition, true);
				if (!emitter)
				{
					continue;
				}

				createdAny = true;
				instance.emitterNames.push_back(BuildEmitterName(effectName, desc, index, true));
				if (desc.burstCount > 0)
				{
					emitter->RequestEmit(desc.burstCount);
				}
			}

			if (!createdAny)
			{
				SetStatus(false, "PlayLoop failed: no supported Sprite emitter. effect=" + effectName);
				return {};
			}

			handleToEffect_[instance.handle.id] = effectName;
			activeLoops_[effectName] = instance;
			SetStatus(true, "Loop started: " + effectName);
			return instance.handle;
		}

		bool StopLoop(const std::string& effectName)
		{
			const auto activeIt = activeLoops_.find(effectName);
			if (activeIt == activeLoops_.end())
			{
				SetStatus(false, "StopLoop skipped: effect is not looping. effect=" + effectName);
				return false;
			}

			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			for (const std::string& emitterName : activeIt->second.emitterNames)
			{
				manager->RemoveEmitter(emitterName);
			}

			handleToEffect_.erase(activeIt->second.handle.id);
			activeLoops_.erase(activeIt);
			SetStatus(true, "Loop stopped: " + effectName);
			return true;
		}

		bool StopLoop(PlayHandle handle)
		{
			if (!handle.IsValid())
			{
				SetStatus(false, "StopLoop skipped: invalid handle.");
				return false;
			}

			const auto handleIt = handleToEffect_.find(handle.id);
			if (handleIt == handleToEffect_.end())
			{
				SetStatus(false, "StopLoop skipped: handle is not active.");
				return false;
			}

			return StopLoop(handleIt->second);
		}

		/// <summary>
		/// 追従演出用にLoop Effect全体の基準位置を更新します。
		/// </summary>
		bool SetLoopPosition(PlayHandle handle, const Vector3& worldPosition)
		{
			const auto handleIt = handleToEffect_.find(handle.id);
			if (handleIt == handleToEffect_.end())
			{
				return false;
			}

			const auto effectIt = effects_.find(handleIt->second);
			const auto loopIt = activeLoops_.find(handleIt->second);
			if (effectIt == effects_.end() || loopIt == activeLoops_.end())
			{
				return false;
			}

			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			const GpuParticleEffectDesc& effect = effectIt->second;
			for (std::size_t index = 0; index < effect.emitters.size(); ++index)
			{
				const GpuParticleEmitterDesc& desc = effect.emitters[index];
				if (desc.renderType != GpuParticleRenderType::Sprite)
				{
					continue;
				}

				if (GpuParticleEmitter* emitter = manager->GetEmitter(BuildEmitterName(effect.effectName, desc, index, true)))
				{
					emitter->SetPosition(Add(worldPosition, desc.position));
				}
			}
			return true;
		}

		[[nodiscard]] bool IsRegistered(const std::string& effectName) const
		{
			return effects_.contains(effectName);
		}

		[[nodiscard]] bool IsLooping(const std::string& effectName) const
		{
			return activeLoops_.contains(effectName);
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

		GpuParticleEffectRuntime() = default;

		[[nodiscard]] const GpuParticleEffectDesc* FindEffect(const std::string& effectName)
		{
			const auto it = effects_.find(effectName);
			if (it == effects_.end())
			{
				SetStatus(false, "Effect is not registered: " + effectName);
				return nullptr;
			}
			return &it->second;
		}

		GpuParticleEmitter* EnsureEmitter(
			const std::string& effectName,
			const GpuParticleEmitterDesc& desc,
			std::size_t emitterIndex,
			const Vector3& worldPosition,
			bool loopMode)
		{
			if (!IsSpawnShapeSupported(desc.spawnShape))
			{
				SetStatus(false, "Runtime currently supports Point/Sphere/Box spawn shapes only. emitter=" + desc.name);
				return nullptr;
			}

			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			const std::string emitterName = BuildEmitterName(effectName, desc, emitterIndex, loopMode);
			if (GpuParticleEmitter* existing = manager->GetEmitter(emitterName))
			{
				existing->SetPosition(Add(worldPosition, desc.position));
				return existing;
			}

			GpuParticleEmitter::EmitterInfo info = CompileEmitterInfo(desc, loopMode);
			GpuParticleEmitter* emitter = manager->CreateRuntimeEmitter(emitterName, info);
			if (!emitter)
			{
				SetStatus(false, "Failed to create runtime emitter: " + emitterName);
				return nullptr;
			}

			emitter->SetPosition(Add(worldPosition, desc.position));
			return emitter;
		}

		static GpuParticleEmitter::EmitterInfo CompileEmitterInfo(const GpuParticleEmitterDesc& desc, bool loopMode)
		{
			GpuParticleEmitter::EmitterInfo info{};
			info.textureFilePath = desc.texturePath.empty() ? "Effects/white.dds" : desc.texturePath;
			info.radius = (std::max)(desc.spawnRadius, 0.0f);
			info.drawType = 0;
			info.kind = GpuParticleKind::Sprite;
			info.spriteType = GpuParticleType::Default;
			info.billboardFlags = desc.billboard ? BillboardMode::Camera : BillboardMode::None;
			info.lifeScale = 1.0f;
			info.speedScale = 1.0f;
			info.useDescSpawnOverride = true;
			info.maxParticles = (std::max)(desc.maxParticles, 1u);
			info.positionRandom = desc.positionRandom;
			info.velocity = desc.velocity;
			info.velocityRandom = desc.velocityRandom;
			info.startSize = desc.startSize;
			info.endSize = desc.endSize;
			info.startColor = desc.startColor;
			info.endColor = desc.endColor;
			info.lifeTime = (std::max)(desc.lifeTime, 0.01f);
			info.lifeTimeRandom = (std::max)(desc.lifeTimeRandom, 0.0f);
			info.gravity = desc.gravity;
			info.damping = (std::max)(desc.damping, 0.0f);
			info.speed = (std::max)(desc.speed, 0.0f);
			info.speedRandom = (std::max)(desc.speedRandom, 0.0f);
			info.sizeRandom = (std::max)(desc.sizeRandom, 0.0f);
			info.startRotation = desc.startRotation;
			info.rotationSpeed = desc.rotationSpeed;
			info.rotationRandom = (std::max)(desc.rotationRandom, 0.0f);
			info.spawnShape = static_cast<uint32_t>(desc.spawnShape);
			info.spawnRadius = (std::max)(desc.spawnRadius, 0.0f);
			info.spawnBoxSize = desc.spawnBoxSize;
			info.colorRandom = desc.colorRandom;
			info.alphaFade = desc.alphaFade;
			info.startScale3D = desc.startScale3D;
			info.endScale3D = desc.endScale3D;
			info.useSpriteSheet = desc.useSpriteSheet;
			info.spriteSheetRows = static_cast<uint32_t>((std::max)(desc.spriteSheetRows, 1));
			info.spriteSheetColumns = static_cast<uint32_t>((std::max)(desc.spriteSheetColumns, 1));
			info.spriteSheetFrameRate = (std::max)(desc.spriteSheetFrameRate, 0.0f);

			if (loopMode && desc.loop && desc.spawnRate > 0.0f)
			{
				const auto [loopCount, loopFrequency] = BuildLoopSchedule(desc.spawnRate);
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
				// 低レートでは1粒ずつ発生させ、秒間発生数をそのまま周期へ変換する。
				return { 1u, 1.0f / spawnRate };
			}

			constexpr float kHighRateTick = 0.10f;
			const uint32_t count = (std::max)(1u, static_cast<uint32_t>(std::lround(spawnRate * kHighRateTick)));
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
			const GpuParticleEmitterDesc& desc,
			std::size_t emitterIndex,
			bool loopMode)
		{
			std::string result = loopMode ? "RuntimeAssetLoop_" : "RuntimeAssetBurst_";
			result += SanitizeName(effectName);
			result += "_" + std::to_string(emitterIndex) + "_" + SanitizeName(desc.name);
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

		void RemoveCachedEmitters(const std::string& effectName)
		{
			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			const auto effectIt = effects_.find(effectName);
			if (effectIt != effects_.end())
			{
				for (std::size_t index = 0; index < effectIt->second.emitters.size(); ++index)
				{
					const GpuParticleEmitterDesc& desc = effectIt->second.emitters[index];
					manager->RemoveEmitter(BuildEmitterName(effectName, desc, index, false));
					manager->RemoveEmitter(BuildEmitterName(effectName, desc, index, true));
				}
			}

			const auto loopIt = activeLoops_.find(effectName);
			if (loopIt != activeLoops_.end())
			{
				handleToEffect_.erase(loopIt->second.handle.id);
				activeLoops_.erase(loopIt);
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

		std::unordered_map<std::string, GpuParticleEffectDesc> effects_;
		std::unordered_map<std::string, std::string> sourcePaths_;
		std::unordered_map<std::string, LoopInstance> activeLoops_;
		std::unordered_map<uint32_t, std::string> handleToEffect_;
		uint32_t nextHandleId_ = 1;
		bool lastOperationSucceeded_ = true;
		std::string lastStatus_ = "Ready";
	};

} // namespace Ken4lowEngine
