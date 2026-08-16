#pragma once

#include "ShaderManifestTypes.h"

#include <cstdint>
#include <stdexcept>

namespace Ken4lowEngine
{
	enum class BladeTrailGraphicsShaderId : uint32_t
	{
		Vertex = 0,
		Pixel,
	};

	class BladeTrailShaderManifest final
	{
	public:
		static const ShaderDescriptor& GetGraphics(BladeTrailGraphicsShaderId id)
		{
			switch (id)
			{
			case BladeTrailGraphicsShaderId::Vertex:
			{
				static const ShaderDescriptor desc{
					L"BladeTrailVS",
					L"Resources/Shaders/BladeTrail/BladeTrail.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::BladeTrail
				};
				return desc;
			}
			case BladeTrailGraphicsShaderId::Pixel:
			{
				static const ShaderDescriptor desc{
					L"BladeTrailPS",
					L"Resources/Shaders/BladeTrail/BladeTrail.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::BladeTrail
				};
				return desc;
			}
			default:
				throw std::runtime_error("BladeTrailShaderManifest::GetGraphics - Unknown id");
			}
		}
	};
}
