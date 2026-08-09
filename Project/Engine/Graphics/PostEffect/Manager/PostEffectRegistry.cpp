#include "PostEffectRegistry.h"

#include "PostEffectRuntimeState.h"
#include "CameraManager.h"
#include "NormalEffect.h"
#include "GrayScaleEffect.h"
#include "VignetteEffect.h"
#include "SmoothingEffect.h"
#include "GaussianFilterEffect.h"
#include "LuminanceOutlineEffect.h"
#include "RadialBlurEffect.h"
#include "DissolveEffect.h"
#include "RandomEffect.h"
#include "AbsorbEffect.h"
#include "DepthOutlineEffect.h"
#include "PixelateEffect.h"
#include "Effects/PlayerHealthPostEffect/PlayerHealthPostEffect.h"
#include "Effects/ToneMappingEffect/ToneMappingEffect.h"
#include "Effects/BloomEffect/BloomEffect.h"

#include <functional>

namespace Ken4lowEngine
{
	namespace
	{
		struct BuiltInEffectRegistration
		{
			PostEffectDefinition definition;
			std::function<std::unique_ptr<IPostEffect>()> creator;
		};

		std::vector<BuiltInEffectRegistration> CreateBuiltInEffectRegistrations()
		{
			using Path = PostEffectExecutionPath;
			return {
				{ { "NormalEffect", true, 0, "Visual", Path::Graphics }, [] { return std::make_unique<NormalEffect>(); } },
				{ { "GrayScaleEffect", false, 1, "Color", Path::Compute }, [] { return std::make_unique<GrayScaleEffect>(); } },
				{ { "VignetteEffect", false, 2, "Color", Path::Compute }, [] { return std::make_unique<VignetteEffect>(); } },
				{ { "SmoothingEffect", false, 3, "Blur", Path::Compute }, [] { return std::make_unique<SmoothingEffect>(); } },
				{ { "GaussianFilterEffect", false, 4, "Blur", Path::Compute }, [] { return std::make_unique<GaussianFilterEffect>(); } },
				{ { "LuminanceOutlineEffect", false, 5, "Visual", Path::Compute }, [] { return std::make_unique<LuminanceOutlineEffect>(); } },
				{ { "RadialBlurEffect", false, 6, "Blur", Path::Compute }, [] { return std::make_unique<RadialBlurEffect>(); } },
				{ { "DissolveEffect", false, 7, "Visual", Path::Compute }, [] { return std::make_unique<DissolveEffect>(); } },
				{ { "RandomEffect", false, 8, "Visual", Path::Compute }, [] { return std::make_unique<RandomEffect>(); } },
				{ { "AbsorbEffect", false, 9, "Visual", Path::Graphics }, [] { return std::make_unique<AbsorbEffect>(); } },
				{ { "DepthOutLineEffect", false, 10, "Visual", Path::Graphics }, [] { return std::make_unique<DepthOutlineEffect>(CameraManager::GetInstance()->GetMainCamera()); } },
				{ { "PixelateEffect", false, 11, "Visual", Path::Compute }, [] { return std::make_unique<PixelateEffect>(); } },
				{ { "PlayerHealthPostEffect", false, 12, "Color", Path::Compute }, [] { return std::make_unique<PlayerHealthPostEffect>(); } },
				{ { "ToneMappingEffect", false, 13, "Color", Path::Compute }, [] { return std::make_unique<ToneMappingEffect>(); } },
				{ { "BloomEffect", false, 14, "Bloom", Path::Compute }, [] { return std::make_unique<BloomEffect>(); } },
			};
		}
	}

	void PostEffectRegistry::Initialize(DirectXCommon* dxCommon, PostEffectPipelineBuilder* pipelineBuilder)
	{
		definitions_.clear();
		effects_.clear();

		for (BuiltInEffectRegistration& registration : CreateBuiltInEffectRegistrations())
		{
			std::unique_ptr<IPostEffect> effect = registration.creator();
			effect->Initialize(dxCommon, pipelineBuilder);
			definitions_.push_back(registration.definition);
			effects_.push_back({ std::move(registration.definition), std::move(effect) });
		}
	}

	void PostEffectRegistry::Finalize()
	{
		for (RegisteredEffect& registered : effects_)
		{
			if (registered.effect)
			{
				registered.effect->Finalize();
			}
		}
		effects_.clear();
		definitions_.clear();
	}

	void PostEffectRegistry::UpdateEditorEnabledEffects(const PostEffectRuntimeState& runtimeState)
	{
		for (RegisteredEffect& registered : effects_)
		{
			if (registered.effect && runtimeState.IsEditorEnabled(registered.definition.name))
			{
				registered.effect->Update(); // 旧Managerと同じくEditor側の有効状態だけでUpdateする。
			}
		}
	}

	IPostEffect* PostEffectRegistry::Find(const std::string& name)
	{
		for (RegisteredEffect& registered : effects_)
		{
			if (registered.definition.name == name)
			{
				return registered.effect.get();
			}
		}
		return nullptr;
	}

	const IPostEffect* PostEffectRegistry::Find(const std::string& name) const
	{
		for (const RegisteredEffect& registered : effects_)
		{
			if (registered.definition.name == name)
			{
				return registered.effect.get();
			}
		}
		return nullptr;
	}

	const PostEffectDefinition* PostEffectRegistry::FindDefinition(const std::string& name) const
	{
		for (const PostEffectDefinition& definition : definitions_)
		{
			if (definition.name == name)
			{
				return &definition;
			}
		}
		return nullptr;
	}
}
