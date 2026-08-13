#pragma once

#include "GpuParticleEffectDesc.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>Emitterがいつ・何個生成するかだけを担当するEmission Moduleです。</summary>
	struct GpuParticleEmissionModule
	{
		uint32_t maxParticles = 1024;
		bool loop = true;
		float duration = 1.0f;
		float spawnRate = 10.0f;
		uint32_t burstCount = 0;
	};

	/// <summary>生成時の位置・寿命・初速度だけを担当するSpawn Moduleです。</summary>
	struct GpuParticleSpawnModule
	{
		GpuParticleSpawnShape shape = GpuParticleSpawnShape::Point;
		Vector3 positionRandom{};
		float radius = 0.0f;
		Vector3 boxSize{};
		float lifeTime = 1.0f;
		float lifeTimeRandom = 0.0f;
		Vector3 velocity{};
		Vector3 velocityRandom{};
		float speed = 0.0f;
		float speedRandom = 0.0f;
	};

	/// <summary>生成後の移動・色・サイズ・回転だけを担当するUpdate Moduleです。</summary>
	struct GpuParticleUpdateModule
	{
		Vector3 gravity{};
		float damping = 0.0f;
		Vector2 startSize{ 1.0f, 1.0f };
		Vector2 endSize{ 1.0f, 1.0f };
		float sizeRandom = 0.0f;
		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		Vector4 colorRandom{};
		bool alphaFade = true;
		float startRotation = 0.0f;
		float rotationSpeed = 0.0f;
		float rotationRandom = 0.0f;
		Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
		Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
		Vector3 angularVelocity{};
		Vector3 angularVelocityRandom{};
	};

	/// <summary>Sprite/Meshの見た目と描画設定だけを担当するRender Moduleです。</summary>
	struct GpuParticleRenderModule
	{
		GpuParticleRenderType renderType = GpuParticleRenderType::Sprite;
		std::string texturePath;
		std::string meshPath;
		bool billboard = true;
		GpuParticleBlendMode blendMode = GpuParticleBlendMode::Alpha;
		bool useSpriteSheet = false;
		int spriteSheetRows = 1;
		int spriteSheetColumns = 1;
		float spriteSheetFrameRate = 0.0f;
	};

	/// <summary>
	/// 1 EmitterをEmission / Spawn / Update / Renderへ分解したRuntime向け中間表現です。
	/// JSON互換の平坦なDescとGPU実装を直接結合せず、今後Module追加やCurve導入をここへ集約します。
	/// </summary>
	struct GpuParticleCompiledEmitter
	{
		std::string name;
		Vector3 localPosition{};
		GpuParticleEmissionModule emission;
		GpuParticleSpawnModule spawn;
		GpuParticleUpdateModule update;
		GpuParticleRenderModule render;
	};

	struct GpuParticleCompiledEffect
	{
		std::string name;
		std::vector<GpuParticleCompiledEmitter> emitters;
	};

	class GpuParticleEffectCompiler
	{
	public:
		[[nodiscard]] static GpuParticleCompiledEmitter CompileEmitter(const GpuParticleEmitterDesc& desc)
		{
			GpuParticleCompiledEmitter out{};
			out.name = desc.name;
			out.localPosition = desc.position;

			// 旧JSONの平坦な値をModule境界へ一度だけ変換し、GPU側へ直接依存させない。
			out.emission.maxParticles = desc.maxParticles;
			out.emission.loop = desc.loop;
			out.emission.duration = desc.duration;
			out.emission.spawnRate = desc.spawnRate;
			out.emission.burstCount = desc.burstCount;

			out.spawn.shape = desc.spawnShape;
			out.spawn.positionRandom = desc.positionRandom;
			out.spawn.radius = desc.spawnRadius;
			out.spawn.boxSize = desc.spawnBoxSize;
			out.spawn.lifeTime = desc.lifeTime;
			out.spawn.lifeTimeRandom = desc.lifeTimeRandom;
			out.spawn.velocity = desc.velocity;
			out.spawn.velocityRandom = desc.velocityRandom;
			out.spawn.speed = desc.speed;
			out.spawn.speedRandom = desc.speedRandom;

			out.update.gravity = desc.gravity;
			out.update.damping = desc.damping;
			out.update.startSize = desc.startSize;
			out.update.endSize = desc.endSize;
			out.update.sizeRandom = desc.sizeRandom;
			out.update.startColor = desc.startColor;
			out.update.endColor = desc.endColor;
			out.update.colorRandom = desc.colorRandom;
			out.update.alphaFade = desc.alphaFade;
			out.update.startRotation = desc.startRotation;
			out.update.rotationSpeed = desc.rotationSpeed;
			out.update.rotationRandom = desc.rotationRandom;
			out.update.startScale3D = desc.startScale3D;
			out.update.endScale3D = desc.endScale3D;
			out.update.angularVelocity = desc.angularVelocity;
			out.update.angularVelocityRandom = desc.angularVelocityRandom;

			out.render.renderType = desc.renderType;
			out.render.texturePath = desc.texturePath;
			out.render.meshPath = desc.meshPath;
			out.render.billboard = desc.billboard;
			out.render.blendMode = desc.blendMode;
			out.render.useSpriteSheet = desc.useSpriteSheet;
			out.render.spriteSheetRows = desc.spriteSheetRows;
			out.render.spriteSheetColumns = desc.spriteSheetColumns;
			out.render.spriteSheetFrameRate = desc.spriteSheetFrameRate;
			return out;
		}

		[[nodiscard]] static GpuParticleCompiledEffect Compile(const GpuParticleEffectDesc& effect)
		{
			GpuParticleCompiledEffect out{};
			out.name = effect.effectName;
			out.emitters.reserve(effect.emitters.size());
			for (const GpuParticleEmitterDesc& emitter : effect.emitters)
			{
				out.emitters.push_back(CompileEmitter(emitter));
			}
			return out;
		}
	};

} // namespace Ken4lowEngine
