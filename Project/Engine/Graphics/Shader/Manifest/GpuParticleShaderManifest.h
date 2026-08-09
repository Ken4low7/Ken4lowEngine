#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	enum class GpuParticleGraphicsShaderId : uint32_t
	{
		SpriteVS,
		SpritePS,
		MeshVS,
		MeshPS,
	};

	enum class GpuParticleComputeShaderId : uint32_t
	{
		SimCS,
		EmitCS,
		UpdateCS,
	};

	class GpuParticleShaderManifest
	{
	public:
		static const ShaderDescriptor& GetGraphics(GpuParticleGraphicsShaderId id)
		{
			switch (id)
			{
			case GpuParticleGraphicsShaderId::SpriteVS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleSpriteVS",
					L"Resources/Shaders/GpuParticle/GpuParticle.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::GpuParticle
				};
				return desc;
			}
			case GpuParticleGraphicsShaderId::SpritePS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleSpritePS",
					L"Resources/Shaders/GpuParticle/GpuParticle.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::GpuParticle
				};
				return desc;
			}
			case GpuParticleGraphicsShaderId::MeshVS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleMeshVS",
					L"Resources/Shaders/GpuParticle/GpuParticleMesh.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::GpuParticle
				};
				return desc;
			}
			case GpuParticleGraphicsShaderId::MeshPS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleMeshPS",
					L"Resources/Shaders/GpuParticle/GpuParticleMesh.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::GpuParticle
				};
				return desc;
			}
			default:
				throw std::runtime_error("GpuParticleShaderManifest::GetGraphics - Unknown id");
			}
		}

		static const ShaderDescriptor& GetCompute(GpuParticleComputeShaderId id)
		{
			switch (id)
			{
			case GpuParticleComputeShaderId::SimCS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleSimCS",
					L"Resources/Shaders/GpuParticle/GpuParticle.CS.hlsl",
					L"main",
					L"cs_6_0",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case GpuParticleComputeShaderId::EmitCS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleEmitCS",
					L"Resources/Shaders/GpuParticle/GpuParticleEmit.CS.hlsl",
					L"main",
					L"cs_6_0",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case GpuParticleComputeShaderId::UpdateCS:
			{
				static const ShaderDescriptor desc{
					L"GpuParticleUpdateCS",
					L"Resources/Shaders/GpuParticle/GpuParticleUpdate.CS.hlsl",
					L"main",
					L"cs_6_0",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			default:
				throw std::runtime_error("GpuParticleShaderManifest::GetCompute - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine