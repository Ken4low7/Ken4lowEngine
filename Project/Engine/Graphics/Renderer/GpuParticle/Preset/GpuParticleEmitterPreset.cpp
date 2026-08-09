#include "GpuParticleEmitterPreset.h"
#include <array>
#include <cassert>

namespace Ken4lowEngine
{
	namespace
	{
		using Preset = GpuParticleEmitterPreset;

		// Default は UI 対象外
		static constexpr std::array<GpuParticleType, static_cast<size_t>(GpuParticleType::Count) - 1> kSpriteTypes =
		{
			GpuParticleType::Blood,
			GpuParticleType::Dust,
			GpuParticleType::Debris,
			GpuParticleType::Smoke,
			GpuParticleType::Ambient,
			GpuParticleType::Spark,
			GpuParticleType::Shockwave,
			GpuParticleType::Heal,
			GpuParticleType::Trail,
			GpuParticleType::DeathBurstCore,
			GpuParticleType::PlayerDamageBlood,
			GpuParticleType::MuzzleFlash,
			GpuParticleType::BulletTracer,
		};

		static const Preset kDefaultPreset =
		{
			"Default",
			"Effects/white.dds",
			0.25f,
			0,
			0.0f,
			0,
			GpuParticleKind::Sprite,
			GpuParticleType::Default,
			GpuRibbonType::Trail,
			BillboardMode::Camera
		};

		static const Preset kSpritePresets[] =
		{
			// Blood
			{
				"Blood",
				"Effects/white.dds",
				0.20f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Blood,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Dust
			{
				"Dust",
				"Effects/white.dds",
				0.22f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Dust,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Debris
			{
				"Debris",
				"Effects/white.dds",
				0.18f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Debris,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Smoke
			{
				"Smoke",
				"Effects/white.dds",
				0.40f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Smoke,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Ambient
			{
				"Ambient",
				"Effects/white.dds",
				0.12f,
				6,
				0.10f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Ambient,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Spark
			{
				"Spark",
				"Effects/white.dds",
				0.10f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Spark,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Shockwave
			{
				"Shockwave",
				"Effects/white.dds",
				0.80f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Shockwave,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Heal
			{
				"Heal",
				"Effects/white.dds",
				0.30f,
				6,
				0.08f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Heal,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// Trail
			{
				"Trail",
				"Effects/white.dds",
				0.16f,
				6,
				0.03f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::Trail,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// DeathBurstCore
			{
				"DeathBurstCore",
				"Effects/white.dds",
				0.28f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::DeathBurstCore,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// PlayerDamageBlood
			{
				"PlayerDamageBlood",
				"Effects/white.dds",
				0.16f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::PlayerDamageBlood,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// MuzzleFlash
			{
				"MuzzleFlash",
				"Effects/white.dds",
				0.08f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::MuzzleFlash,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
			// BulletTracer
			{
				"BulletTracer",
				"Effects/white.dds",
				0.04f,
				0,
				0.0f,
				0,
				GpuParticleKind::Sprite,
				GpuParticleType::BulletTracer,
				GpuRibbonType::Trail,
				BillboardMode::Camera
			},
		};

		static_assert(std::size(kSpritePresets) == std::size(kSpriteTypes));
	}

	const GpuParticleEmitterPreset& GpuParticleEmitterPresetTable::GetSpritePreset(GpuParticleType type)
	{
		if (type == GpuParticleType::Default)
		{
			return kDefaultPreset;
		}

		for (size_t i = 0; i < std::size(kSpriteTypes); ++i)
		{
			if (kSpriteTypes[i] == type)
			{
				return kSpritePresets[i];
			}
		}

		return kDefaultPreset;
	}

	const char* GpuParticleEmitterPresetTable::GetSpriteDisplayName(GpuParticleType type)
	{
		return GetSpritePreset(type).displayName;
	}

	GpuParticleEmitter::EmitterInfo GpuParticleEmitterPresetTable::MakeEmitterInfo(GpuParticleType type)
	{
		const auto& preset = GetSpritePreset(type);

		GpuParticleEmitter::EmitterInfo info{};
		info.textureFilePath = preset.defaultTexture;
		info.radius = preset.defaultRadius;
		info.loopCount = preset.defaultLoopCount;
		info.loopFrequency = preset.defaultLoopFrequency;
		info.drawType = preset.defaultDrawType;
		info.kind = preset.kind;
		info.spriteType = preset.spriteType;
		info.ribbonType = preset.ribbonType;
		info.billboardFlags = preset.billboardFlags;
		return info;
	}

	GpuParticleType GpuParticleEmitterPresetTable::GetSpriteTypeByIndex(uint32_t index)
	{
		assert(index < kSpriteTypes.size());
		return kSpriteTypes[index];
	}

	uint32_t GpuParticleEmitterPresetTable::GetSpriteIndexByType(GpuParticleType type)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(kSpriteTypes.size()); ++i)
		{
			if (kSpriteTypes[i] == type)
			{
				return i;
			}
		}
		return 0;
	}
}