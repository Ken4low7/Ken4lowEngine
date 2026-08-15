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
	DensityDebug,
	TemperatureDebug,
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

	// Phase17.9: Scene Directional LightをVolume scatteringへ反映する品質設定。
	float scatteringStrength = 0.80f;
	float ambientScattering = 0.35f;
	float anisotropy = 0.20f;
	float selfShadowStrength = 0.35f;
	float shadowSampleDistanceCells = 4.0f;

	[[nodiscard]] bool IsValid() const
	{
		return std::isfinite(opacity) && std::isfinite(densityScale) &&
			std::isfinite(temperatureScale) && std::isfinite(absorption) &&
			std::isfinite(emissionStrength) && std::isfinite(stepScale) &&
			std::isfinite(earlyExitTransmittance) &&
			std::isfinite(scatteringStrength) && std::isfinite(ambientScattering) &&
			std::isfinite(anisotropy) && std::isfinite(selfShadowStrength) &&
			std::isfinite(shadowSampleDistanceCells) &&
			opacity >= 0.0f && densityScale >= 0.0f && temperatureScale > 0.0f &&
			absorption >= 0.0f && emissionStrength >= 0.0f && stepScale > 0.0f &&
			earlyExitTransmittance >= 0.0f && earlyExitTransmittance < 1.0f &&
			maxSteps > 0u && maxSteps <= 1024u &&
			scatteringStrength >= 0.0f && ambientScattering >= 0.0f &&
			anisotropy > -0.95f && anisotropy < 0.95f &&
			selfShadowStrength >= 0.0f && selfShadowStrength <= 1.0f &&
			shadowSampleDistanceCells > 0.0f;
	}
};

/// RendererがActive View/Depth/Scene Lightから解決する、描画1回だけの外部状態。
struct GpuVolumetricFluidRenderViewState
{
	Matrix4x4 viewProjection = Matrix4x4::MakeIdentity();
	Matrix4x4 inverseViewProjection = Matrix4x4::MakeIdentity();
	Vector3 cameraPosition{};
	float viewportWidth = 1.0f;
	float viewportHeight = 1.0f;
	float depthClearValue = 1.0f;
	Vector3 directionalLightDirectionToLight{ 0.0f, 1.0f, 0.0f };
	Vector3 directionalLightColor{ 1.0f, 1.0f, 1.0f };
	float directionalLightIntensity = 0.0f;
	Vector3 ambientColor{ 0.10f, 0.10f, 0.10f };
};

/// HLSL GpuVolumetricFluidRenderConstantsと384-byteで一致するDepth-aware Raymarch定数。
struct alignas(16) GpuVolumetricFluidRenderConstants
{
	Matrix4x4 viewProjection = Matrix4x4::MakeIdentity();
	Matrix4x4 inverseViewProjection = Matrix4x4::MakeIdentity();
	Vector4 cameraPositionOpacity{};
	Vector4 domainOriginAbsorption{};
	Vector4 domainAxisUWidth{};
	Vector4 domainAxisVHeight{};
	Vector4 domainAxisWDepth{};
	Vector4 simulationScales{};
	Vector4 emissionEarlyExitStepsMode{};
	Vector4 gridDimensionsShadowDistance{};
	Vector4 depthViewportAnisotropy{};
	Vector4 lightDirectionIntensity{};
	Vector4 lightColorScattering{};
	Vector4 ambientSelfShadow{};
	Vector4 smokeColor{};
	Vector4 coldColor{};
	Vector4 hotColor{};
	Vector4 obstacleColor{};
};
static_assert(sizeof(GpuVolumetricFluidRenderConstants) == 384);

inline GpuVolumetricFluidRenderConstants BuildGpuVolumetricFluidRenderConstants(
	const GpuVolumetricFluidRenderDesc& renderDesc,
	const GpuVolumetricFluidDomainMapping& domain,
	const GpuVolumetricFluidGridDesc& grid,
	const GpuVolumetricFluidRenderViewState& viewState)
{
	const Vector3 axisU = Vector3::NormalizeSafe(domain.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain.axisV, { 0.0f, 1.0f, 0.0f });
	const Vector3 axisW = Vector3::NormalizeSafe(domain.axisW, { 0.0f, 0.0f, 1.0f });
	const Vector3 lightDirection = Vector3::NormalizeSafe(
		viewState.directionalLightDirectionToLight,
		{ 0.0f, 1.0f, 0.0f });
	const float width = static_cast<float>(grid.width) * grid.cellSize;
	const float height = static_cast<float>(grid.height) * grid.cellSize;
	const float depth = static_cast<float>(grid.depth) * grid.cellSize;

	GpuVolumetricFluidRenderConstants constants{};
	constants.viewProjection = viewState.viewProjection;
	constants.inverseViewProjection = viewState.inverseViewProjection;
	constants.cameraPositionOpacity = {
		viewState.cameraPosition.x,
		viewState.cameraPosition.y,
		viewState.cameraPosition.z,
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
	constants.gridDimensionsShadowDistance = {
		static_cast<float>(grid.width),
		static_cast<float>(grid.height),
		static_cast<float>(grid.depth),
		(std::max)(0.01f, renderDesc.shadowSampleDistanceCells)
	};
	constants.depthViewportAnisotropy = {
		(std::max)(1.0f, viewState.viewportWidth),
		(std::max)(1.0f, viewState.viewportHeight),
		viewState.depthClearValue,
		std::clamp(renderDesc.anisotropy, -0.949f, 0.949f)
	};
	constants.lightDirectionIntensity = {
		lightDirection.x,
		lightDirection.y,
		lightDirection.z,
		(std::max)(0.0f, viewState.directionalLightIntensity)
	};
	constants.lightColorScattering = {
		(std::max)(0.0f, viewState.directionalLightColor.x),
		(std::max)(0.0f, viewState.directionalLightColor.y),
		(std::max)(0.0f, viewState.directionalLightColor.z),
		(std::max)(0.0f, renderDesc.scatteringStrength)
	};
	constants.ambientSelfShadow = {
		(std::max)(0.0f, viewState.ambientColor.x + renderDesc.ambientScattering),
		(std::max)(0.0f, viewState.ambientColor.y + renderDesc.ambientScattering),
		(std::max)(0.0f, viewState.ambientColor.z + renderDesc.ambientScattering),
		std::clamp(renderDesc.selfShadowStrength, 0.0f, 1.0f)
	};
	constants.smokeColor = renderDesc.smokeColor;
	constants.coldColor = renderDesc.coldColor;
	constants.hotColor = renderDesc.hotColor;
	constants.obstacleColor = renderDesc.obstacleColor;
	return constants;
}

} // namespace Ken4lowEngine
