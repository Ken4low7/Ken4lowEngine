#pragma once

#include "GpuVolumetricFluidTypes.h"

#include <Vector3.h>

#include <cmath>
#include <cstdint>

namespace Ken4lowEngine
{

enum class GpuVolumetricFluidObstacleShape : uint32_t
{
	Sphere = 0,
	Box,
};

/// Physics Colliderから3D Fluid Rendererへ渡すWorld-space Obstacle Source。
struct GpuVolumetricFluidObstacleSource
{
	GpuVolumetricFluidObstacleShape shape = GpuVolumetricFluidObstacleShape::Sphere;
	Vector3 worldCenter{ 0.0f, 0.0f, 0.0f };
	float radius = 0.5f;
	Vector3 halfSize{ 0.5f, 0.5f, 0.5f };
	Vector3 axisX{ 1.0f, 0.0f, 0.0f };
	Vector3 axisY{ 0.0f, 1.0f, 0.0f };
	Vector3 axisZ{ 0.0f, 0.0f, 1.0f };
	bool enabled = true;

	[[nodiscard]] bool IsValid() const
	{
		if (!enabled)
		{
			return false;
		}

		if (shape == GpuVolumetricFluidObstacleShape::Sphere)
		{
			return radius > 0.0f && std::isfinite(radius);
		}

		return halfSize.x > 0.0f && halfSize.y > 0.0f && halfSize.z > 0.0f &&
			Vector3::LengthSquared(axisX) > 0.0f &&
			Vector3::LengthSquared(axisY) > 0.0f &&
			Vector3::LengthSquared(axisZ) > 0.0f;
	}
};

/// StructuredBufferへ詰める3D Obstacle要素。HLSL側と96-byteで一致させる。
struct alignas(16) GpuVolumetricFluidObstacleGpuData
{
	uint32_t shapeType = 0;
	uint32_t paddingType0 = 0;
	uint32_t paddingType1 = 0;
	uint32_t paddingType2 = 0;

	float centerX = 0.0f;
	float centerY = 0.0f;
	float centerZ = 0.0f;
	float radius = 0.0f;

	float halfSizeX = 0.0f;
	float halfSizeY = 0.0f;
	float halfSizeZ = 0.0f;
	float paddingHalfSize = 0.0f;

	float axisXX = 1.0f;
	float axisXY = 0.0f;
	float axisXZ = 0.0f;
	float paddingAxisX = 0.0f;

	float axisYX = 0.0f;
	float axisYY = 1.0f;
	float axisYZ = 0.0f;
	float paddingAxisY = 0.0f;

	float axisZX = 0.0f;
	float axisZY = 0.0f;
	float axisZZ = 1.0f;
	float paddingAxisZ = 0.0f;
};
static_assert(sizeof(GpuVolumetricFluidObstacleGpuData) == 96);

/// Volume voxel中心をWorldへ戻すRaster定数。HLSL側と64-byteで一致させる。
struct alignas(16) GpuVolumetricFluidObstacleRasterConstants
{
	float originX = 0.0f;
	float originY = 0.0f;
	float originZ = 0.0f;
	uint32_t obstacleCount = 0;

	float axisUX = 1.0f;
	float axisUY = 0.0f;
	float axisUZ = 0.0f;
	float cellSize = 0.0f;

	float axisVX = 0.0f;
	float axisVY = 1.0f;
	float axisVZ = 0.0f;
	float padding0 = 0.0f;

	float axisWX = 0.0f;
	float axisWY = 0.0f;
	float axisWZ = 1.0f;
	float padding1 = 0.0f;
};
static_assert(sizeof(GpuVolumetricFluidObstacleRasterConstants) == 64);

inline bool BuildGpuVolumetricFluidObstacleGpuData(
	const GpuVolumetricFluidObstacleSource& source,
	GpuVolumetricFluidObstacleGpuData& outData)
{
	if (!source.IsValid())
	{
		return false;
	}

	outData = {};
	outData.shapeType = static_cast<uint32_t>(source.shape);
	outData.centerX = source.worldCenter.x;
	outData.centerY = source.worldCenter.y;
	outData.centerZ = source.worldCenter.z;
	outData.radius = source.radius;
	outData.halfSizeX = source.halfSize.x;
	outData.halfSizeY = source.halfSize.y;
	outData.halfSizeZ = source.halfSize.z;

	const Vector3 axisX = Vector3::NormalizeSafe(source.axisX, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisY = Vector3::NormalizeSafe(source.axisY, { 0.0f, 1.0f, 0.0f });
	const Vector3 axisZ = Vector3::NormalizeSafe(source.axisZ, { 0.0f, 0.0f, 1.0f });
	outData.axisXX = axisX.x;
	outData.axisXY = axisX.y;
	outData.axisXZ = axisX.z;
	outData.axisYX = axisY.x;
	outData.axisYY = axisY.y;
	outData.axisYZ = axisY.z;
	outData.axisZX = axisZ.x;
	outData.axisZY = axisZ.y;
	outData.axisZZ = axisZ.z;
	return true;
}

inline bool IntersectsGpuVolumetricFluidDomain(
	const GpuVolumetricFluidObstacleSource& source,
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidGridDesc& grid)
{
	if (!source.IsValid() || !domain.IsValid() || !grid.IsValid())
	{
		return false;
	}

	const Vector3 centerCell = domain.WorldToGrid(source.worldCenter, grid.cellSize);
	float radiusWorld = source.radius;
	if (source.shape == GpuVolumetricFluidObstacleShape::Box)
	{
		// OBBはhalf extentsの長さをBounding Sphereとして使い、Domain外Cullでfalse negativeを出さない。
		radiusWorld = std::sqrt(Vector3::LengthSquared(source.halfSize));
	}
	const float radiusCells = radiusWorld / grid.cellSize;

	return centerCell.x + radiusCells >= 0.0f &&
		centerCell.y + radiusCells >= 0.0f &&
		centerCell.z + radiusCells >= 0.0f &&
		centerCell.x - radiusCells <= static_cast<float>(grid.width) &&
		centerCell.y - radiusCells <= static_cast<float>(grid.height) &&
		centerCell.z - radiusCells <= static_cast<float>(grid.depth);
}

inline GpuVolumetricFluidObstacleRasterConstants BuildGpuVolumetricFluidObstacleRasterConstants(
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidGridDesc& grid,
	uint32_t obstacleCount)
{
	const Vector3 axisU = Vector3::NormalizeSafe(domain.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain.axisV, { 0.0f, 1.0f, 0.0f });
	const Vector3 axisW = Vector3::NormalizeSafe(domain.axisW, { 0.0f, 0.0f, 1.0f });

	GpuVolumetricFluidObstacleRasterConstants constants{};
	constants.originX = domain.origin.x;
	constants.originY = domain.origin.y;
	constants.originZ = domain.origin.z;
	constants.obstacleCount = obstacleCount;
	constants.axisUX = axisU.x;
	constants.axisUY = axisU.y;
	constants.axisUZ = axisU.z;
	constants.cellSize = grid.cellSize;
	constants.axisVX = axisV.x;
	constants.axisVY = axisV.y;
	constants.axisVZ = axisV.z;
	constants.axisWX = axisW.x;
	constants.axisWY = axisW.y;
	constants.axisWZ = axisW.z;
	return constants;
}

} // namespace Ken4lowEngine
