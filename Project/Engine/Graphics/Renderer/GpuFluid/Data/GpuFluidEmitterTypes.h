#pragma once

#include "GpuFluidTypes.h"
#include <Vector2.h>
#include <Vector3.h>

#include <cmath>
#include <cstdint>

namespace Ken4lowEngine
{

/// 2D Fluid Gridを任意のWorld平面へ配置するためのCPU側マッピング。
struct GpuFluidDomainMapping
{
	Vector3 origin{ 0.0f, 0.0f, 0.0f };
	Vector3 axisU{ 1.0f, 0.0f, 0.0f };
	Vector3 axisV{ 0.0f, 1.0f, 0.0f };

	[[nodiscard]] bool IsValid() const
	{
		const Vector3 normalizedU = Vector3::NormalizeSafe(axisU);
		const Vector3 normalizedV = Vector3::NormalizeSafe(axisV);
		return Vector3::LengthSquared(normalizedU) > 0.0f &&
			Vector3::LengthSquared(normalizedV) > 0.0f &&
			std::abs(Vector3::Dot(normalizedU, normalizedV)) < 0.01f;
	}

	[[nodiscard]] Vector2 WorldToGrid(const Vector3& worldPosition, float cellSize) const
	{
		const Vector3 normalizedU = Vector3::NormalizeSafe(axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 normalizedV = Vector3::NormalizeSafe(axisV, { 0.0f, 1.0f, 0.0f });
		const Vector3 offset = worldPosition - origin;
		return {
			Vector3::Dot(offset, normalizedU) / cellSize,
			Vector3::Dot(offset, normalizedV) / cellSize
		};
	}

	[[nodiscard]] Vector2 WorldVelocityToFluid(const Vector3& worldVelocity) const
	{
		const Vector3 normalizedU = Vector3::NormalizeSafe(axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 normalizedV = Vector3::NormalizeSafe(axisV, { 0.0f, 1.0f, 0.0f });
		return {
			Vector3::Dot(worldVelocity, normalizedU),
			Vector3::Dot(worldVelocity, normalizedV)
		};
	}
};

/// Scene ComponentからRendererへ渡す依存の薄いEmitter Sourceデータ。
struct GpuFluidEmitterSource
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
		return radius > 0.0f && velocityStrength >= 0.0f && falloffExponent > 0.0f;
	}
};

/// StructuredBufferへ詰めるGPU側Emitter要素。HLSL側と48-byteで一致させる。
struct alignas(16) GpuFluidEmitterGpuData
{
	float centerCellX = 0.0f;
	float centerCellY = 0.0f;
	float radiusCells = 0.0f;
	float invRadiusCells = 0.0f;

	float velocityX = 0.0f;
	float velocityY = 0.0f;
	float densityRate = 0.0f;
	float temperatureRate = 0.0f;

	float velocityStrength = 0.0f;
	float falloffExponent = 1.0f;
	float padding0 = 0.0f;
	float padding1 = 0.0f;
};
static_assert(sizeof(GpuFluidEmitterGpuData) == 48);

inline bool BuildGpuFluidEmitterGpuData(
	const GpuFluidEmitterSource& source,
	const GpuFluidDomainMapping& domain,
	const GpuFluidGridDesc& grid,
	GpuFluidEmitterGpuData& outData)
{
	if (!source.enabled || !source.IsValid() || !domain.IsValid() || !grid.IsValid())
	{
		return false;
	}

	const Vector2 center = domain.WorldToGrid(source.worldPosition, grid.cellSize);
	const float radiusCells = source.radius / grid.cellSize;
	if (center.x + radiusCells < 0.0f || center.y + radiusCells < 0.0f ||
		center.x - radiusCells > static_cast<float>(grid.width) ||
		center.y - radiusCells > static_cast<float>(grid.height))
	{
		return false; // 完全にGrid外のSourceはUpload配列へ入れず、Shader側ループ数を減らす。
	}

	const Vector2 velocity = domain.WorldVelocityToFluid(source.worldVelocity);
	outData.centerCellX = center.x;
	outData.centerCellY = center.y;
	outData.radiusCells = radiusCells;
	outData.invRadiusCells = 1.0f / radiusCells;
	outData.velocityX = velocity.x;
	outData.velocityY = velocity.y;
	outData.densityRate = source.densityRate;
	outData.temperatureRate = source.temperatureRate;
	outData.velocityStrength = source.velocityStrength;
	outData.falloffExponent = source.falloffExponent;
	return true;
}

} // namespace Ken4lowEngine
