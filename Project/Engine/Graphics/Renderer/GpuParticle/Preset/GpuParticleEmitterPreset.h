#pragma once
#include "GpuParticleEmitter.h"

namespace Ken4lowEngine
{
	struct GpuParticleEmitterPreset
	{
		const char* displayName = "Unknown";
		const char* defaultTexture = "Effects/white.dds";
		float defaultRadius = 0.0f;
		uint32_t defaultLoopCount = 0;
		float defaultLoopFrequency = 0.0f;
		uint32_t defaultDrawType = 0;

		GpuParticleKind kind = GpuParticleKind::Sprite;
		GpuParticleType spriteType = GpuParticleType::Default;
		GpuRibbonType ribbonType = GpuRibbonType::Trail;
		BillboardMode billboardFlags = BillboardMode::Camera;
	};

	class GpuParticleEmitterPresetTable
	{
	public:
		static const GpuParticleEmitterPreset& GetSpritePreset(GpuParticleType type);
		static const char* GetSpriteDisplayName(GpuParticleType type);

		static GpuParticleEmitter::EmitterInfo MakeEmitterInfo(GpuParticleType type);

		static constexpr uint32_t GetSpritePresetCount()
		{
			// Defaultを除いてUIに出す運用
			return static_cast<uint32_t>(GpuParticleType::Count) - 1;
		}

		static GpuParticleType GetSpriteTypeByIndex(uint32_t index);
		static uint32_t GetSpriteIndexByType(GpuParticleType type);
	};
}