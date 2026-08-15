#pragma once

#include "GpuVolumetricFluidTypes.h"

#include <Matrix4x4.h>
#include <Vector3.h>
#include <Vector4.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Ken4lowEngine
{

enum class GpuVolumetricFluidRenderMode : uint32_t
{
	Smoke = 0,
	ObstacleDebug,
};

/// Volume Raymarch専用の見た目設定。Simulation parameterと分離し、描画品質だけを独立調整する。
struct GpuVolumetricFluidRenderDesc
{
	GpuVolumetricFluidRenderMode mode = GpuVolumetricFluidRenderMode::Smoke;
	Vector4 smokeColor{ 0.35f, 0.38f, 0.42f, 1.0f };
	Vector4 coldColor{ 0.10f, 0.35f, 1.00f, 1.0f };
	Vector4 hotColor{ 1.00f, 0.20f, 0.05f, 1.0f };
	Vector4 obstacleColor{ 1.00f, 0.10f, 0.10f, 0.85f };
	float opacity = 1.0f;
	float densityScale = 1.0f;
	float temperatureScale = 1.0f;
	float absorption = 1.5f;
	float emissionStrength = 0.25f;
	float stepScale = 1.0f;
	float earlyExitTransmittance = 0.01f;
	uint32_t maxSteps = 192;

	[[nodiscard]] bool IsValid() const
	{
		return std::isfinite(opacity) && std::isfinite(densityScale) &&
			std::isfinite(temperatureScale) && std::isfinite(absorption) &&
			std::isfinite(emissionStrength) && std::isfinite(stepScale) &&
			std::isfinite(earlyExitTransmittance) &&
			opacity >= 0.0f && densityScale >= 0.0f && temperatureScale > 0.0f &&
			absorption >= 0.0f && emissionStrength >= 0.0f && stepScale > 0.0f &&
			earlyExitTransmittance >= 0.0f && earlyExitTransmittance < 1.0f &&
			maxSteps > 0u && maxSteps <= 1024u;
	}
};

/// HLSL GpuVolumetricFluidRenderConstantsと256-byteで一致するRaymarch定数。
struct alignas(16) GpuVolumetricFluidRenderConstants
{
	Matrix4x4 viewProjection = Matrix4x4::MakeIdentity();
	Vector4 cameraPositionOpacity{};
	Vector4 domainOriginAbsorption{};
	Vector4 domainAxisUWidth{};
	Vector4 domainAxisVHeight{};
	Vector4 domainAxisWDepth{};
	Vector4 simulationScales{};
	Vector4 emissionEarlyExitStepsMode{};
	Vector4 gridDimensionsPadding{};
	Vector4 smokeColor{};
	Vector4 coldColor{};
	Vector4 hotColor{};
	Vector4 obstacleColor{};
};
static_assert(sizeof(GpuVolumetricFluidRenderConstants) == 256);

inline GpuVolumetricFluidRenderConstants BuildGpuVolumetricFluidRenderConstants(
	const GpuVolumetricFluidRenderDesc& renderDesc,
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidGridDesc& grid,
	const Matrix4x4& viewProjection,
	const Vector3& cameraPosition)
{
	const Vector3 axisU = Vector3::NormalizeSafe(domain.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain.axisV, { 0.0f, 1.0f, 0.0f });
	const Vector3 axisW = Vector3::NormalizeSafe(domain.axisW, { 0.0f, 0.0f, 1.0f });
	const float width = static_cast<float>(grid.width) * grid.cellSize;
	const float height = static_cast<float>(grid.height) * grid.cellSize;
	const float depth = static_cast<float>(grid.depth) * grid.cellSize;

	GpuVolumetricFluidRenderConstants constants{};
	constants.viewProjection = viewProjection;
	constants.cameraPositionOpacity = {
		cameraPosition.x,
		cameraPosition.y,
		cameraPosition.z,
		std::clamp(renderDesc.opacity, 0.0f, 1.0f)
	};
	constants.domainOriginAbsorption = {
		domain.origin.x,
		domain.origin.y,
		domain.origin.z,
		(std::max)(0.0f, renderDesc.absorption)
	};
	constants.domainAxisUWidth = { axisU.x, axisU.y, axisU.z, width };
	constants.domainAxisVHeight = { axisV.x, axisV.y, axisV.z, height };
	constants.domainAxisWDepth = { axisW.x, axisW.y, axisW.z, depth };
	constants.simulationScales = {
		grid.cellSize,
		(std::max)(0.0001f, renderDesc.stepScale),
		(std::max)(0.0f, renderDesc.densityScale),
		(std::max)(0.0001f, renderDesc.temperatureScale)
	};
	constants.emissionEarlyExitStepsMode = {
		(std::max)(0.0f, renderDesc.emissionStrength),
		std::clamp(renderDesc.earlyExitTransmittance, 0.0f, 0.9999f),
		static_cast<float>((std::min)(renderDesc.maxSteps, 1024u)),
		static_cast<float>(static_cast<uint32_t>(renderDesc.mode))
	};
	constants.gridDimensionsPadding = {
		static_cast<float>(grid.width),
		static_cast<float>(grid.height),
		static_cast<float>(grid.depth),
		0.0f
	};
	constants.smokeColor = renderDesc.smokeColor;
	constants.coldColor = renderDesc.coldColor;
	constants.hotColor = renderDesc.hotColor;
	constants.obstacleColor = renderDesc.obstacleColor;
	return constants;
}

} // namespace Ken4lowEngine
