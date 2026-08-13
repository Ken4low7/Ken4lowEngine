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

	/// <summary>生成後の移動・色・サイズ・回転・Forceを担当するUpdate Moduleです。</summary>
	struct GpuParticleUpdateModule
	{
		Vector3 gravity{};
		float damping = 0.0f;
		float noiseStrength = 0.0f;
		float noiseFrequency = 1.0f;
		Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f };
		float vortexStrength = 0.0f;
		Vector3 attractorPosition{};
		float attractorStrength = 0.0f;
		float attractorRadius = 0.0f;

		Vector2 startSize{ 1.0f, 1.0f };
		Vector2 endSize{ 1.0f, 1.0f };
		float sizeRandom = 0.0f;
		bool useSizeCurve = false;
		Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f };

		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f };
		Vector4 colorRandom{};
		bool alphaFade = true;
		bool useColorGradient = false;
		std::array<Vector4, 4> colorGradientLut{};

		float startRotation = 0.0f;
		float rotationSpeed = 0.0f;
		float rotationRandom = 0.0f;
		Vector3 startScale3D{ 1.0f, 1.0f, 1.0f };
		Vector3 endScale3D{ 1.0f, 1.0f, 1.0f };
		Vector3 startRotation3D{};
		Vector3 rotationRandom3D{};
		Vector3 angularVelocity{};
		Vector3 angularVelocityRandom{};
	};

	/// <summary>Sprite/Meshの見た目と描画設定だけを担当するRender Moduleです。</summary>
	struct GpuParticleRenderModule
	{
		GpuParticleRenderType renderType = GpuParticleRenderType::Sprite;
		std::string texturePath;
		std::string meshPath;
		uint32_t meshSubMeshIndex = 0;
		bool billboard = true;
		GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive;
		bool useSpriteSheet = false;
		int spriteSheetRows = 1;
		int spriteSheetColumns = 1;
		float spriteSheetFrameRate = 0.0f;
	};

	/// <summary>
	/// 1 EmitterをEmission / Spawn / Update / Renderへ分解したRuntime向け中間表現です。
	/// </summary>
	struct GpuParticleCompiledEmitter
	{
		std::string name;
		Vector3 localPosition{};
		GpuParticleEmissionModule emission;
		GpuParticleSpawnModule spawn;
		GpuParticleUpdateModule update;
		GpuParticleRenderModule render;
		std::vector<GpuParticleParameterBindingDesc> parameterBindings;
	};

	struct GpuParticleCompiledEffect
	{
		std::string name;
		std::vector<GpuParticleUserParameterDesc> userParameters;
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

			// Authoring値をModule境界へ一度だけ変換し、Runtime/GPU側で平坦JSONを直接参照しない。
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
			out.update.noiseStrength = desc.noiseStrength;
			out.update.noiseFrequency = desc.noiseFrequency;
			out.update.vortexAxis = desc.vortexAxis;
			out.update.vortexStrength = desc.vortexStrength;
			out.update.attractorPosition = desc.attractorPosition;
			out.update.attractorStrength = desc.attractorStrength;
			out.update.attractorRadius = desc.attractorRadius;
			out.update.startSize = desc.startSize;
			out.update.endSize = desc.endSize;
			out.update.sizeRandom = desc.sizeRandom;
			out.update.useSizeCurve = desc.useSizeCurve;
			out.update.sizeCurveLut = desc.sizeCurveLut;
			out.update.startColor = desc.startColor;
			out.update.endColor = desc.endColor;
			out.update.colorRandom = desc.colorRandom;
			out.update.alphaFade = desc.alphaFade;
			out.update.useColorGradient = desc.useColorGradient;
			out.update.colorGradientLut = desc.colorGradientLut;
			out.update.startRotation = desc.startRotation;
			out.update.rotationSpeed = desc.rotationSpeed;
			out.update.rotationRandom = desc.rotationRandom;
			out.update.startScale3D = desc.startScale3D;
			out.update.endScale3D = desc.endScale3D;
			out.update.startRotation3D = desc.startRotation3D;
			out.update.rotationRandom3D = desc.rotationRandom3D;
			out.update.angularVelocity = desc.angularVelocity;
			out.update.angularVelocityRandom = desc.angularVelocityRandom;

			out.render.renderType = desc.renderType;
			out.render.texturePath = desc.texturePath;
			out.render.meshPath = desc.meshPath;
			out.render.meshSubMeshIndex = desc.meshSubMeshIndex;
			out.render.billboard = desc.billboard;
			out.render.blendMode = desc.blendMode;
			out.render.useSpriteSheet = desc.useSpriteSheet;
			out.render.spriteSheetRows = desc.spriteSheetRows;
			out.render.spriteSheetColumns = desc.spriteSheetColumns;
			out.render.spriteSheetFrameRate = desc.spriteSheetFrameRate;
			out.parameterBindings = desc.parameterBindings;
			return out;
		}

		[[nodiscard]] static GpuParticleCompiledEffect Compile(const GpuParticleEffectDesc& effect)
		{
			GpuParticleCompiledEffect out{};
			out.name = effect.effectName;
			out.userParameters = effect.userParameters;
			out.emitters.reserve(effect.emitters.size());
			for (const GpuParticleEmitterDesc& emitter : effect.emitters)
			{
				out.emitters.push_back(CompileEmitter(emitter));
			}
			return out;
		}
	};

} // namespace Ken4lowEngine
