#pragma once
#include <cstdint>

namespace Ken4lowEngine
{

	enum class ShaderStage : uint8_t
	{
		Vertex,
		Pixel,
		Compute
	};

	enum class RootSignatureType : uint8_t
	{
		Unknown = 0,
		Object3D,
		Sprite,
		Skinned,
		Particle,
		GpuParticle,
		PostEffect,
		ShadowMap,
		Compute,
		GpuFluid,
		GpuVolumetricFluid,
		BladeTrail, // 既存値を保持したまま武器軌跡専用Graphics契約を末尾へ追加する。
	};

	struct ShaderDescriptor
	{
		const wchar_t* debugName = L"";
		const wchar_t* filePath = L"";
		const wchar_t* entryPoint = L"main";
		const wchar_t* profile = L"";
		ShaderStage stage = ShaderStage::Vertex;
		RootSignatureType rootSignature = RootSignatureType::Unknown;
	};

} // namespace Ken4lowEngine
