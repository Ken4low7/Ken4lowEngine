#pragma once

#include <Vector3.h>

#include <cmath>
#include <cstdint>

namespace Ken4lowEngine
{

/// Phase17の3D Volume Field。Phase16の2D Fieldはそのまま残し、別Resourceとして共存させる。
enum class GpuVolumetricFluidField : uint32_t
{
	Velocity = 0,
	Pressure,
	Divergence,
	Density,
	Temperature,
	Vorticity,
	Obstacle,
	Count
};

struct GpuVolumetricFluidGridDesc
{
	static constexpr uint32_t kMaxDimension = 256;

	uint32_t width = 64;
	uint32_t height = 64;
	uint32_t depth = 64;
	float cellSize = 0.25f;

	[[nodiscard]] bool IsValid() const
	{
		// 3Dは解像度増加が立方で効くため、Resource層へ入る前に各軸256 voxelで上限を固定する。
		return width > 0 && width <= kMaxDimension &&
			height > 0 && height <= kMaxDimension &&
			depth > 0 && depth <= kMaxDimension &&
			cellSize > 0.0f && std::isfinite(cellSize);
	}

	[[nodiscard]] uint64_t GetVoxelCount() const
	{
		return static_cast<uint64_t>(width) *
			static_cast<uint64_t>(height) *
			static_cast<uint64_t>(depth);
	}
};

struct GpuVolumetricFluidSimulationDesc
{
	static constexpr uint32_t kMaxPressureIterations = 192;

	GpuVolumetricFluidGridDesc grid{};
	float fixedDeltaTime = 1.0f / 60.0f;
	uint32_t pressureIterations = 32;
	uint32_t maxSubsteps = 2;
	float velocityDissipation = 0.995f;
	float densityDissipation = 0.999f;
	float temperatureDissipation = 0.995f;
	float vorticityStrength = 0.15f;
	float ambientTemperature = 0.0f;
	float buoyancy = 1.0f;
	float smokeWeight = 0.05f;

	[[nodiscard]] bool IsValid() const
	{
		return grid.IsValid() &&
			fixedDeltaTime > 0.0f && std::isfinite(fixedDeltaTime) &&
			pressureIterations > 0 && pressureIterations <= kMaxPressureIterations &&
			maxSubsteps > 0 &&
			velocityDissipation >= 0.0f && velocityDissipation <= 1.0f &&
			densityDissipation >= 0.0f && densityDissipation <= 1.0f &&
			temperatureDissipation >= 0.0f && temperatureDissipation <= 1.0f;
	}
};

/// 3D GridのローカルUVW軸をWorldへ配置するMapping。originはVolumeの最小UVW cornerを表す。
struct GpuVolumetricFluidDomainMapping
{
	Vector3 origin{ 0.0f, 0.0f, 0.0f };
	Vector3 axisU{ 1.0f, 0.0f, 0.0f };
	Vector3 axisV{ 0.0f, 1.0f, 0.0f };
	Vector3 axisW{ 0.0f, 0.0f, 1.0f };

	[[nodiscard]] bool IsValid() const
	{
		const Vector3 u = Vector3::NormalizeSafe(axisU);
		const Vector3 v = Vector3::NormalizeSafe(axisV);
		const Vector3 w = Vector3::NormalizeSafe(axisW);
		return Vector3::LengthSquared(u) > 0.0f &&
			Vector3::LengthSquared(v) > 0.0f &&
			Vector3::LengthSquared(w) > 0.0f &&
			std::abs(Vector3::Dot(u, v)) < 0.01f &&
			std::abs(Vector3::Dot(u, w)) < 0.01f &&
			std::abs(Vector3::Dot(v, w)) < 0.01f;
	}

	[[nodiscard]] Vector3 WorldToGrid(const Vector3& worldPosition, float cellSize) const
	{
		const Vector3 u = Vector3::NormalizeSafe(axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 v = Vector3::NormalizeSafe(axisV, { 0.0f, 1.0f, 0.0f });
		const Vector3 w = Vector3::NormalizeSafe(axisW, { 0.0f, 0.0f, 1.0f });
		const Vector3 offset = worldPosition - origin;
		return {
			Vector3::Dot(offset, u) / cellSize,
			Vector3::Dot(offset, v) / cellSize,
			Vector3::Dot(offset, w) / cellSize
		};
	}

	[[nodiscard]] Vector3 GridToWorld(const Vector3& gridPosition, float cellSize) const
	{
		const Vector3 u = Vector3::NormalizeSafe(axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 v = Vector3::NormalizeSafe(axisV, { 0.0f, 1.0f, 0.0f });
		const Vector3 w = Vector3::NormalizeSafe(axisW, { 0.0f, 0.0f, 1.0f });
		return origin +
			u * (gridPosition.x * cellSize) +
			v * (gridPosition.y * cellSize) +
			w * (gridPosition.z * cellSize);
	}

	[[nodiscard]] Vector3 WorldVelocityToFluid(const Vector3& worldVelocity) const
	{
		const Vector3 u = Vector3::NormalizeSafe(axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 v = Vector3::NormalizeSafe(axisV, { 0.0f, 1.0f, 0.0f });
		const Vector3 w = Vector3::NormalizeSafe(axisW, { 0.0f, 0.0f, 1.0f });
		return {
			Vector3::Dot(worldVelocity, u),
			Vector3::Dot(worldVelocity, v),
			Vector3::Dot(worldVelocity, w)
		};
	}
};

/// HLSL側GpuVolumetricFluidSimulationConstantsと16-byte境界まで明示的に一致させる。
struct alignas(16) GpuVolumetricFluidSimulationConstants
{
	uint32_t gridWidth = 0;
	uint32_t gridHeight = 0;
	uint32_t gridDepth = 0;
	float cellSize = 0.0f;

	float invGridWidth = 0.0f;
	float invGridHeight = 0.0f;
	float invGridDepth = 0.0f;
	float invCellSize = 0.0f;

	float deltaTime = 0.0f;
	float elapsedTime = 0.0f;
	float velocityDissipation = 1.0f;
	float densityDissipation = 1.0f;

	float temperatureDissipation = 1.0f;
	float vorticityStrength = 0.0f;
	float ambientTemperature = 0.0f;
	float buoyancy = 0.0f;

	float smokeWeight = 0.0f;
	float padding0 = 0.0f;
	float padding1 = 0.0f;
	float padding2 = 0.0f;
};
static_assert(sizeof(GpuVolumetricFluidSimulationConstants) == 80);

inline GpuVolumetricFluidSimulationConstants BuildGpuVolumetricFluidSimulationConstants(
	const GpuVolumetricFluidSimulationDesc& desc,
	float deltaTime,
	float elapsedTime)
{
	GpuVolumetricFluidSimulationConstants constants{};
	constants.gridWidth = desc.grid.width;
	constants.gridHeight = desc.grid.height;
	constants.gridDepth = desc.grid.depth;
	constants.cellSize = desc.grid.cellSize;
	constants.invGridWidth = 1.0f / static_cast<float>(desc.grid.width);
	constants.invGridHeight = 1.0f / static_cast<float>(desc.grid.height);
	constants.invGridDepth = 1.0f / static_cast<float>(desc.grid.depth);
	constants.invCellSize = 1.0f / desc.grid.cellSize;
	constants.deltaTime = deltaTime;
	constants.elapsedTime = elapsedTime;
	constants.velocityDissipation = desc.velocityDissipation;
	constants.densityDissipation = desc.densityDissipation;
	constants.temperatureDissipation = desc.temperatureDissipation;
	constants.vorticityStrength = desc.vorticityStrength;
	constants.ambientTemperature = desc.ambientTemperature;
	constants.buoyancy = desc.buoyancy;
	constants.smokeWeight = desc.smokeWeight;
	return constants;
}

} // namespace Ken4lowEngine
