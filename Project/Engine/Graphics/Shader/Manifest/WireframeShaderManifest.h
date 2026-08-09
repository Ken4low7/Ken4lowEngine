#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	enum class WireframeShaderId : uint32_t
	{
		WireframeVS,
		WireframeAABBInstancedVS,
		WireframePS,
	};

	class WireframeShaderManifest
	{
	public:
		static const ShaderDescriptor& Get(WireframeShaderId id)
		{
			switch (id)
			{
			case WireframeShaderId::WireframeVS:
			{
				static const ShaderDescriptor desc{
					L"WireframeVS",
					L"Resources/Shaders/Wireframe/Wireframe.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Object3D
				};
				return desc;
			}
			case WireframeShaderId::WireframePS:
			{
				static const ShaderDescriptor desc{
					L"WireframePS",
					L"Resources/Shaders/Wireframe/Wireframe.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::Object3D
				};
				return desc;
			}
			case WireframeShaderId::WireframeAABBInstancedVS:
			{
				static const ShaderDescriptor desc{
					L"WireframeAABBInstancedVS",
					L"Resources/Shaders/Wireframe/WireframeAABBInstanced.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Object3D
				};
				return desc;
			}
			default:
				throw std::runtime_error("WireframeShaderManifest::Get - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine
