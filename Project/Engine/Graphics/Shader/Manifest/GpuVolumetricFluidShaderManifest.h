#pragma once

#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

enum class GpuVolumetricFluidComputeShaderId : uint32_t
{
	VelocityAdvection = 0,
	Divergence,
	PressureJacobi,
	Projection,
};

/// Phase17の3D Fluid ShaderをPhase16の2D Manifestから分離し、両Solverを独立して拡張する。
class GpuVolumetricFluidShaderManifest
{
public:
	static const ShaderDescriptor& GetCompute(GpuVolumetricFluidComputeShaderId id)
	{
		switch (id)
		{
		case GpuVolumetricFluidComputeShaderId::VelocityAdvection:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidVelocityAdvectionCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVelocityAdvection.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::Divergence:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidDivergenceCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidDivergence.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::PressureJacobi:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidPressureJacobiCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidPressureJacobi.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::Projection:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidProjectionCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidProjection.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		default:
			throw std::runtime_error("GpuVolumetricFluidShaderManifest::GetCompute - Unknown id");
		}
	}
};

} // namespace Ken4lowEngine
