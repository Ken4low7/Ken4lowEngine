#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///                Sprite 用 Shader ID
	/// -------------------------------------------------------------
	enum class SpriteShaderId : uint32_t
	{
		SpriteVS,
		SpritePS,
	};

	/// -------------------------------------------------------------
	///              Sprite 用 Shader Manifest
	/// -------------------------------------------------------------
	class SpriteShaderManifest
	{
	public:
		static const ShaderDescriptor& Get(SpriteShaderId id)
		{
			switch (id)
			{
			case SpriteShaderId::SpriteVS:
			{
				static const ShaderDescriptor desc{
					L"SpriteVS",
					L"Resources/Shaders/Sprite/Sprite.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Sprite
				};
				return desc;
			}
			case SpriteShaderId::SpritePS:
			{
				static const ShaderDescriptor desc{
					L"SpritePS",
					L"Resources/Shaders/Sprite/Sprite.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::Sprite
				};
				return desc;
			}
			default:
				throw std::runtime_error("SpriteShaderManifest::Get - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine