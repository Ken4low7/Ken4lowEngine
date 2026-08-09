#pragma once
#include "ShaderManifestTypes.h"
#include <stdexcept>

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///            PostEffect 用 Graphics Shader ID
	/// -------------------------------------------------------------
	enum class PostEffectGraphicsShaderId : uint32_t
	{
		FullscreenVS,

		NormalPS,
		AbsorbPS,
		DepthOutlinePS,
	};

	/// -------------------------------------------------------------
	///            PostEffect 用 Compute Shader ID
	/// -------------------------------------------------------------
	enum class PostEffectComputeShaderId : uint32_t
	{
		GrayScaleCS,
		GaussianFilterCS,
		VignetteCS,
		RadialBlurCS,
		PixelateCS,
		RandomCS,
		SmoothingCS,
		DissolveCS,
		LuminanceOutlineCS,
		PlayerHealthCS,
		ToneMappingCS,
		BloomCS,
	};

	/// -------------------------------------------------------------
	///          PostEffect 用 Shader Manifest
	/// -------------------------------------------------------------
	class PostEffectShaderManifest
	{
	public:
		/// <summary>
		/// Graphics 用シェーダー契約を取得します。
		/// </summary>
		static const ShaderDescriptor& GetGraphics(PostEffectGraphicsShaderId id)
		{
			switch (id)
			{
			case PostEffectGraphicsShaderId::FullscreenVS:
			{
				static const ShaderDescriptor desc{
					L"PostEffectFullscreenVS",
					L"Resources/Shaders/PostEffect/FullScreen.VS.hlsl",
					L"main",
					L"vs_6_0",
					ShaderStage::Vertex,
					RootSignatureType::PostEffect
				};
				return desc;
			}
			case PostEffectGraphicsShaderId::NormalPS:
			{
				static const ShaderDescriptor desc{
					L"NormalEffectPS",
					L"Resources/Shaders/PostEffect/NormalEffect.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::PostEffect
				};
				return desc;
			}
			case PostEffectGraphicsShaderId::AbsorbPS:
			{
				static const ShaderDescriptor desc{
					L"AbsorbEffectPS",
					L"Resources/Shaders/PostEffect/AbsorbEffect.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::PostEffect
				};
				return desc;
			}
			case PostEffectGraphicsShaderId::DepthOutlinePS:
			{
				static const ShaderDescriptor desc{
					L"DepthOutlineEffectPS",
					L"Resources/Shaders/PostEffect/DepthOutlineEffect.PS.hlsl",
					L"main",
					L"ps_6_0",
					ShaderStage::Pixel,
					RootSignatureType::PostEffect
				};
				return desc;
			}
			default:
				throw std::runtime_error("PostEffectShaderManifest::GetGraphics - Unknown id");
			}
		}

		/// <summary>
		/// Compute 用シェーダー契約を取得します。
		/// </summary>
		static const ShaderDescriptor& GetCompute(PostEffectComputeShaderId id)
		{
			switch (id)
			{
			case PostEffectComputeShaderId::GrayScaleCS:
			{
				static const ShaderDescriptor desc{
					L"GrayScaleEffectCS",
					L"Resources/Shaders/PostEffect/GrayScaleEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::GaussianFilterCS:
			{
				static const ShaderDescriptor desc{
					L"GaussianFilterEffectCS",
					L"Resources/Shaders/PostEffect/GaussianFilterEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::VignetteCS:
			{
				static const ShaderDescriptor desc{
					L"VignetteEffectCS",
					L"Resources/Shaders/PostEffect/VignetteEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::RadialBlurCS:
			{
				static const ShaderDescriptor desc{
					L"RadialBlurEffectCS",
					L"Resources/Shaders/PostEffect/RadialBlurEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::PixelateCS:
			{
				static const ShaderDescriptor desc{
					L"PixelateEffectCS",
					L"Resources/Shaders/PostEffect/PixelateEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::RandomCS:
			{
				static const ShaderDescriptor desc{
					L"RandomEffectCS",
					L"Resources/Shaders/PostEffect/RandomEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::SmoothingCS:
			{
				static const ShaderDescriptor desc{
					L"SmoothingEffectCS",
					L"Resources/Shaders/PostEffect/SmoothingEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::DissolveCS:
			{
				static const ShaderDescriptor desc{
					L"DissolveEffectCS",
					L"Resources/Shaders/PostEffect/DissolveEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::LuminanceOutlineCS:
			{
				static const ShaderDescriptor desc{
					L"LuminanceOutlineEffectCS",
					L"Resources/Shaders/PostEffect/LuminanceOutlineEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}

			case PostEffectComputeShaderId::PlayerHealthCS:
			{
				static const ShaderDescriptor desc{
					L"PlayerHealthPostEffectCS",
					L"Resources/Shaders/PostEffect/PlayerHealthPostEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::ToneMappingCS:
			{
				static const ShaderDescriptor desc{
					L"ToneMappingEffectCS",
					L"Resources/Shaders/PostEffect/ToneMappingEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			case PostEffectComputeShaderId::BloomCS:
			{
				static const ShaderDescriptor desc{
					L"BloomEffectCS",
					L"Resources/Shaders/PostEffect/BloomEffect.CS.hlsl",
					L"main",
					L"cs_6_6",
					ShaderStage::Compute,
					RootSignatureType::Compute
				};
				return desc;
			}
			default:
				throw std::runtime_error("PostEffectShaderManifest::GetCompute - Unknown id");
			}
		}
	};

} // namespace Ken4lowEngine
