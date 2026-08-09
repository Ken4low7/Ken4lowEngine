#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	enum class ParticleShaderId : uint32_t
	{
		ParticleVS,
		ParticlePS,
	};

	class ParticleShaderManifest
	{
	public:
		static const ShaderDescriptor& Get(ParticleShaderId id)
		{
			switch (id)
			{
			case ParticleShaderId::ParticleVS:
			{
				static const ShaderDescriptor desc{
					L"ParticleVS",
					L"Resources/Shaders/Particle/Particle.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::Particle
				};
				return desc;
			}
			case ParticleShaderId::ParticlePS:
			{
				static const ShaderDescriptor desc{
					L"ParticlePS",
					L"Resources/Shaders/Particle/Particle.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::Particle
				};
				return desc;
			}
			default:
				throw std::runtime_error("ParticleShaderManifest::Get - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine