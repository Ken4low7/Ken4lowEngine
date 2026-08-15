#pragma once

#include <cstdint>

namespace Ken4lowEngine
{

// Phase 17で3Dへ拡張できるよう、シミュレーション設定とGPUリソース定義を分離する。
enum class GpuFluidField : uint32_t
{
	Velocity = 0,
	Pressure,
	Divergence,
	Density,
	Temperature,
	// Vorticityを独立Fieldとして持ち、圧力用DivergenceとCurl中間値の用途を混在させない。
	Vorticity,
	Obstacle,
	Count
};

struct GpuFluidGridDesc
{
	uint32_t width = 256;
	uint32_t height = 256;
	float cellSize = 0.1f;

	[[nodiscard]] bool IsValid() const
	{
		return width > 0 && height > 0 && cellSize > 0.0f;
	}
};

struct GpuFluidSimulationDesc
{
	GpuFluidGridDesc grid{};
	float fixedDeltaTime = 1.0f / 60.0f;
	uint32_t pressureIterations = 40;
	uint32_t maxSubsteps = 4;
	float velocityDissipation = 0.995f;
	float densityDissipation = 0.999f;
	float temperatureDissipation = 0.995f;
	float vorticityStrength = 0.25f;
	float ambientTemperature = 0.0f;
	float buoyancy = 1.0f;
	float smokeWeight = 0.05f;

	[[nodiscard]] bool IsValid() const
	{
		return grid.IsValid() &&
			fixedDeltaTime > 0.0f &&
			pressureIterations > 0 &&
			maxSubsteps > 0 &&
			velocityDissipation >= 0.0f && velocityDissipation <= 1.0f &&
			densityDissipation >= 0.0f && densityDissipation <= 1.0f &&
			temperatureDissipation >= 0.0f && temperatureDissipation <= 1.0f;
	}
};

struct alignas(16) GpuFluidSimulationConstants
{
	uint32_t gridWidth = 0;
	uint32_t gridHeight = 0;
	float invGridWidth = 0.0f;
	float invGridHeight = 0.0f;

	float cellSize = 0.0f;
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
	float padding = 0.0f;
};
static_assert(sizeof(GpuFluidSimulationConstants) == 64);

inline GpuFluidSimulationConstants BuildGpuFluidSimulationConstants(
	const GpuFluidSimulationDesc& desc,
	float deltaTime,
	float elapsedTime)
{
	GpuFluidSimulationConstants constants{};
	constants.gridWidth = desc.grid.width;
	constants.gridHeight = desc.grid.height;
	constants.invGridWidth = 1.0f / static_cast<float>(desc.grid.width);
	constants.invGridHeight = 1.0f / static_cast<float>(desc.grid.height);
	constants.cellSize = desc.grid.cellSize;
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
