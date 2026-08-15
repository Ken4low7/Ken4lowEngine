#pragma once

#include "GpuVolumetricFluidTypes.h"
#include <Vector3.h>

#include <cmath>
#include <cstdint>

namespace Ken4lowEngine
{

/// Scene Componentから3D Volume Solverへ渡すWorld-space Emitter Source。
struct GpuVolumetricFluidEmitterSource
{
	Vector3 worldPosition{ 0.0f, 0.0f, 0.0f };
	Vector3 worldVelocity{ 0.0f, 1.0f, 0.0f };
	float radius = 0.5f;
	float velocityStrength = 1.0f;
	float densityRate = 1.0f;
	float temperatureRate = 1.0f;
	float falloffExponent = 2.0f;
	bool enabled = true;

	[[nodiscard]] bool IsValid() const
	{
		return radius > 0.0f && std::isfinite(radius) &&
			velocityStrength >= 0.0f && std::isfinite(velocityStrength) &&
			std::isfinite(densityRate) && std::isfinite(temperatureRate) &&
			falloffExponent > 0.0f && std::isfinite(falloffExponent);
	}
};

/// HLSL StructuredBuffer要素と64-byteで一致させ、3D Sourceを16-byte境界で保持する。
struct alignas(16) GpuVolumetricFluidEmitterGpuData
{
	float centerCellX = 0.0f;
	float centerCellY = 0.0f;
	float centerCellZ = 0.0f;
	float radiusCells = 0.0f;

	float velocityX = 0.0f;
	float velocityY = 0.0f;
	float velocityZ = 0.0f;
	float velocityStrength = 0.0f;

	float densityRate = 0.0f;
	float temperatureRate = 0.0f;
	float falloffExponent = 1.0f;
	float invRadiusCells = 0.0f;

	float padding0 = 0.0f;
	float padding1 = 0.0f;
	float padding2 = 0.0f;
	float padding3 = 0.0f;
};
static_assert(sizeof(GpuVolumetricFluidEmitterGpuData) == 64);

inline bool BuildGpuVolumetricFluidEmitterGpuData(
	const GpuVolumetricFluidEmitterSource& source,
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidGridDesc& grid,
	GpuVolumetricFluidEmitterGpuData& outData)
{
	if (!source.enabled || !source.IsValid() || !domain.IsValid() || !grid.IsValid())
	{
		return false;
	}

	const Vector3 center = domain.WorldToGrid(source.worldPosition, grid.cellSize);
	const float radiusCells = source.radius / grid.cellSize;
	if (center.x + radiusCells < 0.0f || center.y + radiusCells < 0.0f || center.z + radiusCells < 0.0f ||
		center.x - radiusCells > static_cast<float>(grid.width) ||
		center.y - radiusCells > static_cast<float>(grid.height) ||
		center.z - radiusCells > static_cast<float>(grid.depth))
	{
		return false; // 完全にVolume外の球SourceはUpload前に除外し、voxelごとのEmitter loopを減らす。
	}

	const Vector3 velocity = domain.WorldVelocityToFluid(source.worldVelocity);
	outData.centerCellX = center.x;
	outData.centerCellY = center.y;
	outData.centerCellZ = center.z;
	outData.radiusCells = radiusCells;
	outData.velocityX = velocity.x;
	outData.velocityY = velocity.y;
	outData.velocityZ = velocity.z;
	outData.velocityStrength = source.velocityStrength;
	outData.densityRate = source.densityRate;
	outData.temperatureRate = source.temperatureRate;
	outData.falloffExponent = source.falloffExponent;
	outData.invRadiusCells = 1.0f / radiusCells;
	return true;
}

} // namespace Ken4lowEngine
