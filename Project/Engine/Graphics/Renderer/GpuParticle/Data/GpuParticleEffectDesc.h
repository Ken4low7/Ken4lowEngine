#pragma once

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
	/// 既存のGpuParticleKindはGPUとの互換用に残し、この列挙型では編集・保存対象を2種類だけに限定します。
	/// </summary>
	enum class GpuParticleRenderType : uint32_t
	{
		Sprite = 0,
		Mesh = 1,
	};

	/// <summary>Spriteの合成方法を切り替えるための設定です。Preview Runtime未接続の値もJSON/Editorでは保持します。</summary>
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

	/// <summary>
	/// 1つのGPUパーティクルEmitterを編集・保存するための設定です。
	/// 今回は既存描画処理へ直結せず、次Phaseでランタイム設定へ変換するための土台として追加します。
	/// JSONで項目名をそのまま表現しやすいよう、描画リソースと基本的な生成・変化パラメータを平坦に保持します。
	/// </summary>
	struct GpuParticleEmitterDesc
	{
		std::string name = "Emitter"; ///< Emitterの識別名です。
		GpuParticleRenderType renderType = GpuParticleRenderType::Sprite; ///< SpriteまたはMeshの描画方式です。

		std::string texturePath; ///< Spriteで使用するテクスチャです。Meshでも将来のマテリアル拡張用に保持します。
		std::string meshPath; ///< Meshで使用するモデルです。Spriteでは使用しません。

		uint32_t maxParticles = 1024; ///< このEmitterが確保する最大パーティクル数です。
		bool loop = true; ///< trueならdurationを超えてもspawnRateによる生成を継続します。
		float duration = 1.0f; ///< loop=falseのEmitterが生成を続ける時間（秒）です。
		float spawnRate = 10.0f; ///< 1秒あたりの生成数です。
		uint32_t burstCount = 16; ///< 再生開始時やPreview Emit Countが0のときの一括生成数です。
		float lifeTime = 1.0f; ///< 生成されたパーティクルの寿命（秒）です。
		float lifeTimeRandom = 0.0f; ///< lifeTimeに加える±ランダム幅（秒）です。

		Vector3 position{ 0.0f, 0.0f, 0.0f }; ///< Emitterの基準位置です。
		Vector3 positionRandom{ 0.0f, 0.0f, 0.0f }; ///< 基準位置に加えるランダム範囲です。
		GpuParticleSpawnShape spawnShape = GpuParticleSpawnShape::Point; ///< Point/Sphere/Boxの発生分布です。
		float spawnRadius = 0.0f; ///< Sphere形状の半径です。
		Vector3 spawnBoxSize{ 0.0f, 0.0f, 0.0f }; ///< Box形状の各軸の全幅です。

		Vector3 velocity{ 0.0f, 0.0f, 0.0f }; ///< 初速度です。
		Vector3 velocityRandom{ 0.0f, 0.0f, 0.0f }; ///< 初速度に加えるランダム範囲です。
		Vector3 gravity{ 0.0f, 0.0f, 0.0f }; ///< 毎秒加える重力加速度です。
		float damping = 0.0f; ///< 速度を減衰させる係数です。
		float speed = 0.0f; ///< 0より大きい場合、velocity方向へ適用する速さです。
		float speedRandom = 0.0f; ///< speedに加える±ランダム幅です。

		Vector2 startSize{ 1.0f, 1.0f }; ///< 生成時の幅と高さです。
		Vector2 endSize{ 1.0f, 1.0f }; ///< 寿命終了時の幅と高さです。
		float sizeRandom = 0.0f; ///< 開始・終了サイズへ乗算する±ランダム幅です。

		Vector4 startColor{ 1.0f, 1.0f, 1.0f, 1.0f }; ///< 生成時のRGBAカラーです。
		Vector4 endColor{ 1.0f, 1.0f, 1.0f, 0.0f }; ///< 寿命終了時のRGBAカラーです。
		Vector4 colorRandom{ 0.0f, 0.0f, 0.0f, 0.0f }; ///< 開始色へ加える各RGBAの±ランダム幅です。
		bool alphaFade = true; ///< trueなら寿命比でstartColor.aからendColor.aへ補間します。

		float startRotation = 0.0f; ///< Sprite生成時のZ回転（ラジアン）です。
		float rotationSpeed = 0.0f; ///< Spriteの毎秒Z回転速度（ラジアン）です。
		float rotationRandom = 0.0f; ///< startRotationに加える±ランダム幅です。

		bool billboard = true; ///< Spriteをカメラへ向けます。Meshでは通常使用しません。
		GpuParticleBlendMode blendMode = GpuParticleBlendMode::Alpha; ///< Spriteの合成方法です。
		bool useSpriteSheet = false; ///< SpriteSheetアニメーションを使用する予定の設定です。
		int spriteSheetRows = 1; ///< SpriteSheetの行数です。
		int spriteSheetColumns = 1; ///< SpriteSheetの列数です。
		float spriteSheetFrameRate = 0.0f; ///< SpriteSheetの再生FPSです。

		Vector3 startScale3D{ 1.0f, 1.0f, 1.0f }; ///< Mesh生成時の3Dスケールです。
		Vector3 endScale3D{ 1.0f, 1.0f, 1.0f }; ///< Mesh寿命終了時の3Dスケールです。
		Vector3 angularVelocity{ 0.0f, 0.0f, 0.0f }; ///< Meshの回転速度（Runtime未接続）です。
		Vector3 angularVelocityRandom{ 0.0f, 0.0f, 0.0f }; ///< Mesh回転速度のランダム幅（Runtime未接続）です。
	};

	/// <summary>
	/// 1つの演出を構成するGPUパーティクルEffect設定です。
	/// 炎・煙・破片など役割の異なる複数Emitterを1つのEffectとして再生・編集できるよう、Emitter配列を保持します。
	/// </summary>
	struct GpuParticleEffectDesc
	{
		std::string effectName = "Effect"; ///< Effectの識別名です。
		std::vector<GpuParticleEmitterDesc> emitters; ///< 同時に扱うSprite/Mesh Emitterの一覧です。
	};

	/// <summary>
	/// 新規Sprite Emitter作成時や、JSONで不足している項目の補完に使う既定値を生成します。
	/// SpriteはtexturePathを使用し、カメラへ向くbillboardを既定で有効にします。
	/// </summary>
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
		desc.blendMode = GpuParticleBlendMode::Alpha;
		desc.billboard = true;
		return desc;
	}

	/// <summary>
	/// 新規Mesh Emitter作成時や、JSONで不足している項目の補完に使う既定値を生成します。
	/// MeshはmeshPathを使用し、板ポリゴンではないためbillboardを既定で無効にします。
	/// </summary>
	inline GpuParticleEmitterDesc CreateDefaultMeshEmitterDesc()
	{
		GpuParticleEmitterDesc desc{};
		desc.name = "MeshEmitter";
		desc.renderType = GpuParticleRenderType::Mesh;
		desc.texturePath = "Effects/white.dds";
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
		desc.spawnShape = GpuParticleSpawnShape::Sphere;
		desc.blendMode = GpuParticleBlendMode::Alpha;
		desc.billboard = false;
		return desc;
	}

	/// <summary>
	/// 新規Effect作成時や、JSONで不足している項目の補完に使う安全な既定値を生成します。
	/// 編集開始直後から扱えるよう、Effectには既定のSprite Emitterを1つ持たせます。
	/// </summary>
	inline GpuParticleEffectDesc CreateDefaultGpuParticleEffectDesc()
	{
		GpuParticleEffectDesc desc{};
		desc.effectName = "NewGpuParticleEffect";
		desc.emitters.push_back(CreateDefaultSpriteEmitterDesc());
		return desc;
	}

} // namespace Ken4lowEngine
