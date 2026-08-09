#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

	// 既存GPU/HLSLとの互換を維持する低レベル分類です。
	// 新しい編集・JSON設計ではGpuParticleRenderType（Sprite/Meshのみ）を使用し、今回はこの既存分類を削除しません。
	enum class GpuParticleKind : uint32_t
	{
		Sprite = 0,
		Mesh = 1,
		Ribbon = 2,
		Beam = 3,
	};

	// --- pack（HLSLと一致）---
	static constexpr uint32_t GPU_PARTICLE_KIND_SHIFT = 16;
	static constexpr uint32_t GPU_PARTICLE_KIND_MASK = 0x00FF0000u;
	static constexpr uint32_t GPU_PARTICLE_BB_MASK = 0x0000FFFFu;

	static constexpr uint32_t PackBillboardMode(GpuParticleKind kind, uint32_t bbFlags)
	{
		return ((static_cast<uint32_t>(kind) << GPU_PARTICLE_KIND_SHIFT) & GPU_PARTICLE_KIND_MASK) |
			(bbFlags & GPU_PARTICLE_BB_MASK);
	}

	/// -------------------------------------------------------------
	/// GPUパーティクルの演出プリセット（Blood/Smokeなど）を管理する既存列挙型
	/// 描画方式ではないため、新設計のGpuParticleRenderTypeとは役割を分けて段階的に接続します。
	/// -------------------------------------------------------------
	enum class GpuParticleType : uint32_t
	{
		Default = 0,

		Blood,
		Dust,
		Debris,
		Smoke,
		Ambient,
		Spark,
		Shockwave,
		Heal,
		Trail,
		DeathBurstCore,

		// 相互評価で多かった「被弾」「射撃」「弾道」の手応えを出すための追加タイプ
		PlayerDamageBlood,
		MuzzleFlash,
		BulletTracer,

		Count
	};

	enum class GpuRibbonType : uint32_t
	{
		Trail = 0,
	};

	// RibbonType → 既存の SpawnByType 用 type にマップ（当面はこれでOK）
	static constexpr GpuParticleType ToGpuParticleType(GpuRibbonType t)
	{
		switch (t)
		{
		case GpuRibbonType::Trail:  return GpuParticleType::Trail;
		default:                    return GpuParticleType::Trail;
		}
	}
} // namespace Ken4lowEngine
