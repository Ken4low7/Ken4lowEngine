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
	ScalarAdvection,
	VorticityCurl,
	VorticityConfinement,
	Buoyancy,
	EmitterInjection,
	ObstacleRaster,
};

enum class GpuVolumetricFluidGraphicsShaderId : uint32_t
{
	RaymarchVS = 0,
	RaymarchPS,
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
		case GpuVolumetricFluidComputeShaderId::ScalarAdvection:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidScalarAdvectionCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidScalarAdvection.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::VorticityCurl:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidVorticityCurlCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVorticityCurl.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::VorticityConfinement:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidVorticityConfinementCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidVorticityConfinement.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::Buoyancy:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidBuoyancyCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidBuoyancy.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::EmitterInjection:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidEmitterInjectionCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidEmitterInjection.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuVolumetricFluidComputeShaderId::ObstacleRaster:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidObstacleRasterCS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidObstacleRaster.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		default:
			throw std::runtime_error("GpuVolumetricFluidShaderManifest::GetCompute - Unknown id");
		}
	}

	static const ShaderDescriptor& GetGraphics(GpuVolumetricFluidGraphicsShaderId id)
	{
		switch (id)
		{
		case GpuVolumetricFluidGraphicsShaderId::RaymarchVS:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidRaymarchVS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.VS.hlsl",
				L"main", L"vs_6_0", ShaderStage::Vertex, RootSignatureType::GpuVolumetricFluid
			};
			return desc;
		}
		case GpuVolumetricFluidGraphicsShaderId::RaymarchPS:
		{
			static const ShaderDescriptor desc{
				L"GpuVolumetricFluidRaymarchPS",
				L"Resources/Shaders/GpuFluid/Volumetric/GpuVolumetricFluidRaymarch.PS.hlsl",
				L"main", L"ps_6_0", ShaderStage::Pixel, RootSignatureType::GpuVolumetricFluid
			};
			return desc;
		}
		default:
			throw std::runtime_error("GpuVolumetricFluidShaderManifest::GetGraphics - Unknown id");
		}
	}
};

} // namespace Ken4lowEngine
