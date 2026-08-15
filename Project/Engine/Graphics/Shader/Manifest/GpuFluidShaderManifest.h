#pragma once

#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

enum class GpuFluidComputeShaderId : uint32_t
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

enum class GpuFluidGraphicsShaderId : uint32_t
{
	ForwardVS = 0,
	ForwardPS,
};

class GpuFluidShaderManifest
{
public:
	static const ShaderDescriptor& GetCompute(GpuFluidComputeShaderId id)
	{
		switch (id)
		{
		case GpuFluidComputeShaderId::VelocityAdvection:
		{
			// Fluid Compute Shaderも既存ShaderCompiler経路へ統一し、個別DXC呼び出しを増やさない。
			static const ShaderDescriptor desc{
				L"GpuFluidVelocityAdvectionCS",
				L"Resources/Shaders/GpuFluid/GpuFluidVelocityAdvection.CS.hlsl",
				L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute
			};
			return desc;
		}
		case GpuFluidComputeShaderId::Divergence:
		{
			static const ShaderDescriptor desc{ L"GpuFluidDivergenceCS", L"Resources/Shaders/GpuFluid/GpuFluidDivergence.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::PressureJacobi:
		{
			static const ShaderDescriptor desc{ L"GpuFluidPressureJacobiCS", L"Resources/Shaders/GpuFluid/GpuFluidPressureJacobi.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::Projection:
		{
			static const ShaderDescriptor desc{ L"GpuFluidProjectionCS", L"Resources/Shaders/GpuFluid/GpuFluidProjection.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::ScalarAdvection:
		{
			// DensityとTemperatureは同じScalar Advection Shaderを共有し、PSOの重複を避ける。
			static const ShaderDescriptor desc{ L"GpuFluidScalarAdvectionCS", L"Resources/Shaders/GpuFluid/GpuFluidScalarAdvection.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::VorticityCurl:
		{
			static const ShaderDescriptor desc{ L"GpuFluidVorticityCurlCS", L"Resources/Shaders/GpuFluid/GpuFluidVorticityCurl.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::VorticityConfinement:
		{
			static const ShaderDescriptor desc{ L"GpuFluidVorticityConfinementCS", L"Resources/Shaders/GpuFluid/GpuFluidVorticityConfinement.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::Buoyancy:
		{
			static const ShaderDescriptor desc{ L"GpuFluidBuoyancyCS", L"Resources/Shaders/GpuFluid/GpuFluidBuoyancy.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::EmitterInjection:
		{
			static const ShaderDescriptor desc{ L"GpuFluidEmitterInjectionCS", L"Resources/Shaders/GpuFluid/GpuFluidEmitterInjection.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		case GpuFluidComputeShaderId::ObstacleRaster:
		{
			// Physics Collider群をR8_UINT Solid Maskへ毎Step再構築し、動的Obstacleにも同じ経路を使う。
			static const ShaderDescriptor desc{ L"GpuFluidObstacleRasterCS", L"Resources/Shaders/GpuFluid/GpuFluidObstacleRaster.CS.hlsl", L"main", L"cs_6_0", ShaderStage::Compute, RootSignatureType::Compute };
			return desc;
		}
		default:
			throw std::runtime_error("GpuFluidShaderManifest::GetCompute - Unknown id");
		}
	}

	static const ShaderDescriptor& GetGraphics(GpuFluidGraphicsShaderId id)
	{
		switch (id)
		{
		case GpuFluidGraphicsShaderId::ForwardVS:
		{
			// World-space QuadはSV_VertexIDで生成し、Fluid専用Vertex Bufferを不要にする。
			static const ShaderDescriptor desc{
				L"GpuFluidForwardVS",
				L"Resources/Shaders/GpuFluid/GpuFluidForward.VS.hlsl",
				L"main", L"vs_6_0", ShaderStage::Vertex, RootSignatureType::GpuFluid
			};
			return desc;
		}
		case GpuFluidGraphicsShaderId::ForwardPS:
		{
			static const ShaderDescriptor desc{
				L"GpuFluidForwardPS",
				L"Resources/Shaders/GpuFluid/GpuFluidForward.PS.hlsl",
				L"main", L"ps_6_0", ShaderStage::Pixel, RootSignatureType::GpuFluid
			};
			return desc;
		}
		default:
			throw std::runtime_error("GpuFluidShaderManifest::GetGraphics - Unknown id");
		}
	}
};

} // namespace Ken4lowEngine
