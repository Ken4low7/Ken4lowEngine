#pragma once

#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

enum class GpuFluidComputeShaderId : uint32_t
{
	VelocityAdvection = 0,
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
				L"main",
				L"cs_6_0",
				ShaderStage::Compute,
				RootSignatureType::Compute
			};
			return desc;
		}
		default:
			throw std::runtime_error("GpuFluidShaderManifest::GetCompute - Unknown id");
		}
	}
};

} // namespace Ken4lowEngine
