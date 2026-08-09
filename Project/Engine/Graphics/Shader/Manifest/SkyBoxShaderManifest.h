#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	enum class SkyBoxShaderId : uint32_t
	{
		SkyBoxVS,
		SkyBoxPS,
	};

	class SkyBoxShaderManifest
	{
	public:
		static const ShaderDescriptor& Get(SkyBoxShaderId id)
		{
			switch (id)
			{
			case SkyBoxShaderId::SkyBoxVS:
			{
				static const ShaderDescriptor desc{
					L"SkyBoxVS",
					L"Resources/Shaders/SkyBox/SkyBox.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Object3D
				};
				return desc;
			}
			case SkyBoxShaderId::SkyBoxPS:
			{
				static const ShaderDescriptor desc{
					L"SkyBoxPS",
					L"Resources/Shaders/SkyBox/SkyBox.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::Object3D
				};
				return desc;
			}
			default:
				throw std::runtime_error("SkyBoxShaderManifest::Get - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine