#pragma once

#include "GpuFluidEmitterTypes.h"
#include "GpuFluidTypes.h"

#include <Matrix4x4.h>
#include <Vector3.h>
#include <Vector4.h>

#include <algorithm>
#include <cstdint>

namespace Ken4lowEngine
{

enum class GpuFluidRenderMode : uint32_t
{
	Density = 0,
	Temperature,
	Obstacle,
};

/// Forward描画だけが使う見た目設定。Simulation parameterとは分離してEditor調整を安全にする。
struct GpuFluidRenderDesc
{
	GpuFluidRenderMode mode = GpuFluidRenderMode::Density;
	Vector4 smokeColor{ 0.35f, 0.38f, 0.42f, 1.0f };
	Vector4 coldColor{ 0.10f, 0.35f, 1.00f, 1.0f };
	Vector4 hotColor{ 1.00f, 0.20f, 0.05f, 1.0f };
	Vector4 obstacleColor{ 1.00f, 0.10f, 0.10f, 0.70f };
	float opacity = 1.0f;
	float densityScale = 1.0f;
	float temperatureScale = 1.0f;

	[[nodiscard]] bool IsValid() const
	{
		return opacity >= 0.0f && densityScale >= 0.0f && temperatureScale > 0.0f;
	}
};

/// HLSL FluidForwardCBと192-byteで一致するWorld-space Quad描画定数。
struct alignas(16) GpuFluidRenderConstants
{
	Matrix4x4 viewProjection = Matrix4x4::MakeIdentity();
	Vector4 domainOriginOpacity{};
	Vector4 domainAxisUDensityScale{};
	Vector4 domainAxisVTemperatureScale{};
	Vector4 smokeColor{};
	Vector4 coldColor{};
	Vector4 hotColor{};
	Vector4 obstacleColor{};
	uint32_t gridWidth = 0;
	uint32_t gridHeight = 0;
	uint32_t renderMode = 0;
	uint32_t padding = 0;
};
static_assert(sizeof(GpuFluidRenderConstants) == 192);

inline GpuFluidRenderConstants BuildGpuFluidRenderConstants(
	const GpuFluidRenderDesc& renderDesc,
	const GpuFluidDomainMapping& domain,
	const GpuFluidGridDesc& grid,
	const Matrix4x4& viewProjection)
{
	GpuFluidRenderConstants constants{};
	constants.viewProjection = viewProjection;

	const Vector3 axisU = Vector3::NormalizeSafe(domain.axisU, { 1.0f, 0.0f, 0.0f });
	const Vector3 axisV = Vector3::NormalizeSafe(domain.axisV, { 0.0f, 1.0f, 0.0f });
	const float widthWorld = static_cast<float>(grid.width) * grid.cellSize;
	const float heightWorld = static_cast<float>(grid.height) * grid.cellSize;
	const Vector3 extentU = axisU * widthWorld;
	const Vector3 extentV = axisV * heightWorld;

	constants.domainOriginOpacity = {
		domain.origin.x,
		domain.origin.y,
		domain.origin.z,
		std::clamp(renderDesc.opacity, 0.0f, 1.0f)
	};
	constants.domainAxisUDensityScale = {
		extentU.x,
		extentU.y,
		extentU.z,
		(std::max)(0.0f, renderDesc.densityScale)
	};
	constants.domainAxisVTemperatureScale = {
		extentV.x,
		extentV.y,
		extentV.z,
		(std::max)(0.0001f, renderDesc.temperatureScale)
	};
	constants.smokeColor = renderDesc.smokeColor;
	constants.coldColor = renderDesc.coldColor;
	constants.hotColor = renderDesc.hotColor;
	constants.obstacleColor = renderDesc.obstacleColor;
	constants.gridWidth = grid.width;
	constants.gridHeight = grid.height;
	constants.renderMode = static_cast<uint32_t>(renderDesc.mode);
	return constants;
}

} // namespace Ken4lowEngine
