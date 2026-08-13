#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include <Vector2.h>
#include <Vector3.h>
#include <Vector4.h>

namespace Ken4lowEngine
{

	/// <summary>
	/// 新しいGPUパーティクル設計で扱う描画方式です。
	/// Spriteは板ポリゴンとtexturePathを、MeshはモデルとmeshPath（必要に応じてtexturePathも）を使用します。
	/// </summary>
	enum class GpuParticleRenderType : uint32_t
	{
		Sprite = 0,
		Mesh = 1,
	};

	/// <summary>Sprite/Meshの合成方法を切り替えるための設定です。</summary>
	enum class GpuParticleBlendMode : uint32_t
	{
		Alpha = 0,
		Additive,
		Multiply,
	};

	/// <summary>Emitterを基準に、パーティクル発生位置の分布を選ぶための設定です。</summary>
	enum class GpuParticleSpawnShape : uint32_t
	{
		Point = 0,
		Sphere,
		Box,
		Cone,
		Circle,
		Ring,
		Hemisphere,
	};

	/// <summary>User ParameterがどのEmitter特性へ倍率として作用するかを表します。</summary>
	enum class GpuParticleParameterTarget : uint32_t
	{
		SpawnRate = 0,
		BurstCount,
		LifeTime,
		Speed,
		Size,
		Alpha,
		Force,
	};

	/// <summary>Effect単位で公開するfloat User Parameterです。</summary>
	struct GpuParticleUserParameterDesc
	{
		std::string name = "Parameter";
		float defaultValue = 1.0f;
		float minValue = 0.0f;
		float maxValue = 10.0f;
	};

	/// <summary>Parameter値をscale/biasで倍率へ変換し、Emitterの指定Targetへ適用します。</summary>
	struct GpuParticleParameterBindingDesc
	{
		std::string parameterName;
		GpuParticleParameterTarget target = GpuParticleParameterTarget::Speed;
		float scale = 1.0f;
		float bias = 0.0f;
	};

	/// <summary>
	/// 1つのGPUパーティクルEmitterを編集・保存するための設定です。
	/// 既存GPU描画処理とはCompilerを介して接続し、Authoring側では演出用パラメータを保持します。
	/// </summary>
	struct GpuParticleEmitterDesc
	{
		std::string name = "Emitter"; ///< Emitterの識別名です。
		GpuParticleRenderType renderType = GpuParticleRenderType::Sprite; ///< SpriteまたはMeshの描画方式です。

		std::string texturePath; ///< Spriteで使用するテクスチャです。Meshでは空ならモデル側テクスチャを使用します。
		std::string meshPath; ///< Meshで使用するモデルです。Spriteでは使用しません。
		uint32_t meshSubMeshIndex = 0; ///< Meshモデル内で使用するsubMesh indexです。

		uint32_t maxParticles = 1024; ///< このEmitterが確保する最大パーティクル数です。
		bool loop = true; ///< trueならspawnRateによる生成を継続します。
		float duration = 1.0f; ///< loop=falseのEmitterが生成を続ける時間（秒）です。
		float spawnRate = 10.0f; ///< 1秒あたりの生成数です。
		uint32_t burstCount = 16; ///< 再生開始時の一括生成数です。
		float lifeTime = 1.0f; ///< 生成されたパーティクルの寿命（秒）です。
		float lifeTimeRandom = 0.0f; ///< lifeTimeに加える±ランダム幅（秒）です。

		Vector3 position{ 0.0f, 0.0f, 0.0f }; ///< Emitterの基準位置です。
		Vector3 positionRandom{ 0.0f, 0.0f, 0.0f }; ///< 基準位置に加えるランダム範囲です。
		GpuParticleSpawnShape spawnShape = GpuParticleSpawnShape::Point; ///< 発生分布です。
		float spawnRadius = 0.0f; ///< Sphere/Cone/Circle/Ring/Hemisphereで使用する半径です。
		Vector3 spawnBoxSize{ 0.0f, 0.0f, 0.0f }; ///< Boxの各軸全幅。ConeではYを高さとして使用します。

		Vector3 velocity{ 0.0f, 0.0f, 0.0f }; ///< 初速度です。
		Vector3 velocityRandom{ 0.0f, 0.0f, 0.0f }; ///< 初速度に加えるランダム範囲です。
		Vector3 gravity{ 0.0f, 0.0f, 0.0f }; ///< 毎秒加える重力加速度です。
		float damping = 0.0f; ///< 速度を減衰させる係数です。
		float speed = 0.0f; ///< 0より大きい場合、velocity方向へ適用する速さです。
		float speedRandom = 0.0f; ///< speedに加える±ランダム幅です。

		// Noise/Vortex/Attractorを同じUpdate Moduleで合成し、特殊演出も新しいParticleType追加なしで作れるようにする。
		float noiseStrength = 0.0f; ///< 擬似Noise加速度の強さです。
		float noiseFrequency = 1.0f; ///< Noise空間周波数です。
		Vector3 vortexAxis{ 0.0f, 1.0f, 0.0f }; ///< Vortexの回転軸です。
		float vortexStrength = 0.0f; ///< Vortexの接線方向加速度です。
		Vector3 attractorPosition{ 0.0f, 0.0f, 0.0f }; ///< Emitter基準のAttractorローカル位置です。
		float attractorStrength = 0.0f; ///< Attractorへ引く加速度です。
		float attractorRadius = 0.0f; ///< 0なら無限範囲、0より大きければ範囲外で無効です。

		Vector2 startSize{ 1.0f, 1.0f }; ///< 生成時の幅と高さです。
		Vector2 endSize{ 1.0f, 1.0f }; ///< 寿命終了時の幅と高さです。
		float sizeRandom = 0.0f; ///< 開始・終了サイズへ乗算する±ランダム幅です。
		bool useSizeCurve = false; ///< trueなら4点GPU LUTをstart/end補間へ乗算します。
		Vector4 sizeCurveLut{ 1.0f, 1.0f, 1.0f, 1.0f }; ///< t=0,1/3,2/3,1のSize倍率です。

		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f }; ///< 生成時のRGBAカラーです。
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f }; ///< 寿命終了時のRGBAカラーです。
		Vector4 colorRandom{ 0.0f, 0.0f, 0.0f, 0.0f }; ///< 開始色へ加える各RGBAの±ランダム幅です。
		bool alphaFade = true; ///< falseならGradient未使用時のalphaを開始値へ固定します。
		bool useColorGradient = false; ///< trueなら4点GPU Gradient LUTを使用します。
		std::array<Vector4, 4> colorGradientLut{
			Vector4{ 1.0f, 1.0f, 1.0f, 1.0f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.66f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.33f },
			Vector4{ 1.0f, 1.0f, 1.0f, 0.0f }
		}; ///< t=0,1/3,2/3,1のRGBAです。

		float startRotation = 0.0f; ///< Sprite生成時のZ回転（ラジアン）です。
		float rotationSpeed = 0.0f; ///< Spriteの毎秒Z回転速度（ラジアン）です。
		float rotationRandom = 0.0f; ///< startRotationに加える±ランダム幅です。

		bool billboard = true; ///< Spriteをカメラへ向けます。Meshでは通常使用しません。
		GpuParticleBlendMode blendMode = GpuParticleBlendMode::Additive; ///< Alpha/Additive/MultiplyをPSO切替で描画します。
		bool useSpriteSheet = false; ///< SpriteSheetアニメーションを使用します。
		int spriteSheetRows = 1; ///< SpriteSheetの行数です。
		int spriteSheetColumns = 1; ///< SpriteSheetの列数です。
		float spriteSheetFrameRate = 0.0f; ///< SpriteSheetの再生FPSです。

		Vector3 startScale3D{ 1.0f, 1.0f, 1.0f }; ///< Mesh生成時の3Dスケールです。
		Vector3 endScale3D{ 1.0f, 1.0f, 1.0f }; ///< Mesh寿命終了時の3Dスケールです。
		Vector3 startRotation3D{ 0.0f, 0.0f, 0.0f }; ///< Mesh生成時のXYZ Euler回転（ラジアン）です。
		Vector3 rotationRandom3D{ 0.0f, 0.0f, 0.0f }; ///< Mesh開始回転の±ランダム幅です。
		Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f }; ///< MeshのXYZ回転速度（ラジアン/秒）です。
		Vector3 angularVelocityRandom{ 0.0f, 0.0f, 0.0f }; ///< Mesh回転速度の±ランダム幅です。

		std::vector<GpuParticleParameterBindingDesc> parameterBindings; ///< Effect User Parameterとの接続一覧です。
	};

	/// <summary>
	/// 1つの演出を構成するGPUパーティクルEffect設定です。
	/// </summary>
	struct GpuParticleEffectDesc
	{
		std::string effectName = "Effect"; ///< Effectの識別名です。
		std::vector<GpuParticleUserParameterDesc> userParameters; ///< Gameplayへ公開するfloat Parameter一覧です。
		std::vector<GpuParticleEmitterDesc> emitters; ///< 同時に扱うSprite/Mesh Emitterの一覧です。
	};

	inline GpuParticleEmitterDesc CreateDefaultSpriteEmitterDesc()
	{
		GpuParticleEmitterDesc desc{};
		desc.name = "SpriteEmitter";
		desc.renderType = GpuParticleRenderType::Sprite;
		desc.texturePath = "Effects/white.dds";
		desc.maxParticles = 1024;
		desc.loop = true;
		desc.duration = 1.0f;
		desc.spawnRate = 50.0f;
		desc.burstCount = 32;
		desc.lifeTime = 2.0f;
		desc.velocity = { 0.0f, 2.0f, 0.0f };
		desc.velocityRandom = { 1.0f, 1.0f, 1.0f };
		desc.gravity = { 0.0f, -2.0f, 0.0f };
		desc.startSize = { 0.1f, 0.1f };
		desc.endSize = { 0.8f, 0.8f };
		desc.startColor = { 1.0f, 0.8f, 0.2f, 1.0f };
		desc.endColor = { 1.0f, 0.0f, 0.0f, 0.0f };
		desc.spawnShape = GpuParticleSpawnShape::Point;
		desc.blendMode = GpuParticleBlendMode::Additive;
		desc.billboard = true;
		return desc;
	}

	inline GpuParticleEmitterDesc CreateDefaultMeshEmitterDesc()
	{
		GpuParticleEmitterDesc desc{};
		desc.name = "MeshEmitter";
		desc.renderType = GpuParticleRenderType::Mesh;
		desc.texturePath.clear();
		desc.meshPath = "Sample/cube.gltf";
		desc.maxParticles = 256;
		desc.loop = false;
		desc.duration = 1.0f;
		desc.spawnRate = 10.0f;
		desc.burstCount = 8;
		desc.lifeTime = 2.0f;
		desc.velocity = { 0.0f, 2.0f, 0.0f };
		desc.gravity = { 0.0f, -9.8f, 0.0f };
		desc.startScale3D = { 0.2f, 0.2f, 0.2f };
		desc.endScale3D = { 0.1f, 0.1f, 0.1f };
		desc.angularVelocity = { 0.0f, 2.0f, 0.0f };
		desc.spawnShape = GpuParticleSpawnShape::Sphere;
		desc.blendMode = GpuParticleBlendMode::Alpha;
		desc.billboard = false;
		return desc;
	}

	inline GpuParticleEffectDesc CreateDefaultGpuParticleEffectDesc()
	{
		GpuParticleEffectDesc desc{};
		desc.effectName = "NewGpuParticleEffect";
		desc.emitters.push_back(CreateDefaultSpriteEmitterDesc());
		return desc;
	}

} // namespace Ken4lowEngine
