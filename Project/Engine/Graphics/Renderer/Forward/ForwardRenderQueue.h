#pragma once

#include "Engine/Graphics/Common/BlendModeType.h"
#include "Engine/Graphics/Material/Material.h"

#include <cstdint>

namespace Ken4lowEngine
{
	enum class ForwardRenderBucket : uint8_t
	{
		Opaque = 0,
		Masked,
		Transparent,
		Additive,
		Count,
	};

	enum class ForwardSortMode : uint8_t
	{
		FrontToBack = 0,
		BackToFront,
	};

	/// <summary>
	/// Materialの高レベルなBlend分類からForward passの不変条件を決定します。
	/// </summary>
	struct ForwardRenderPolicy
	{
		ForwardRenderBucket bucket = ForwardRenderBucket::Opaque;
		BlendMode blendMode = BlendMode::kBlendModeNone;
		ForwardSortMode sortMode = ForwardSortMode::FrontToBack;
		bool depthWrite = true;
		bool alphaTest = false;
	};

	constexpr ForwardRenderPolicy ResolveForwardRenderPolicy(MaterialBlendMode blendMode)
	{
		switch (blendMode)
		{
		case MaterialBlendMode::Masked:
			return { ForwardRenderBucket::Masked, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, true };
		case MaterialBlendMode::Transparent:
			return { ForwardRenderBucket::Transparent, BlendMode::kBlendModeNormal, ForwardSortMode::BackToFront, false, false };
		case MaterialBlendMode::Additive:
			return { ForwardRenderBucket::Additive, BlendMode::kBlendModeAdd, ForwardSortMode::BackToFront, false, false };
		case MaterialBlendMode::Opaque:
		default:
			return { ForwardRenderBucket::Opaque, BlendMode::kBlendModeNone, ForwardSortMode::FrontToBack, true, false };
		}
	}

	constexpr bool IsForwardTransparentBucket(ForwardRenderBucket bucket)
	{
		return bucket == ForwardRenderBucket::Transparent || bucket == ForwardRenderBucket::Additive;
	}
} // namespace Ken4lowEngine
