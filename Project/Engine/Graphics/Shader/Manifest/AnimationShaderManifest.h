#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	enum class AnimationGraphicsShaderId : uint32_t
	{
		SkinningObject3DVS,
		SkinningObject3DPS,
	};

	enum class AnimationComputeShaderId : uint32_t
	{
		SkinningObject3DCS,
	};

	class AnimationShaderManifest
	{
	public:
		static const ShaderDescriptor& GetGraphics(AnimationGraphicsShaderId id)
		{
			switch (id)
			{
			case AnimationGraphicsShaderId::SkinningObject3DVS:
			{
				static const ShaderDescriptor desc{
					L"SkinningObject3DVS",
					L"Resources/Shaders/Skinning/SkinningObject3d.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Skinned
				};
				return desc;
			}
			case AnimationGraphicsShaderId::SkinningObject3DPS:
			{
				static const ShaderDescriptor desc{
					L"SkinningObject3DPS",
					L"Resources/Shaders/Skinning/SkinningObject3d.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::Skinned
				};
				return desc;
			}
			default:
				throw std::runtime_error("AnimationShaderManifest::GetGraphics - Unknown id");
			}
		}

		static const ShaderDescriptor& GetCompute(AnimationComputeShaderId id)
		{
			switch (id)
			{
			case AnimationComputeShaderId::SkinningObject3DCS:
			{
				static const ShaderDescriptor desc{
					L"SkinningObject3DCS",
					L"Resources/Shaders/Skinning/SkinningObject3d.CS.hlsl",
					L"main",
					L"cs_6_0",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			default:
				throw std::runtime_error("AnimationShaderManifest::GetCompute - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine