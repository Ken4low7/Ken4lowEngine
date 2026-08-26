#pragma once

#include <Vector3.h>

#include <cstdint>

namespace Ken4lowEngine
{

/// GPU SPHの1粒子をCPU/HLSLで共有するための固定レイアウト。
struct GpuSphParticle
{
	Vector3 position{};
	float density = 0.0f;
	Vector3 velocity{};
	float pressure = 0.0f;
	Vector3 predictedPosition{};
	float padding = 0.0f;
};

// float4境界へ揃え、StructuredBufferのCPU/HLSL strideを固定する。
static_assert(sizeof(GpuSphParticle) == 48);

struct GpuSphParticleBufferStats
{
	uint32_t capacity = 0;
	uint32_t activeCount = 0;
	uint32_t strideBytes = sizeof(GpuSphParticle);
	uint64_t approximateGpuMemoryBytes = 0;
	bool initialized = false;
};

} // namespace Ken4lowEngine
