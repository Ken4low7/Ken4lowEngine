#pragma once

#include "Engine/Graphics/Renderer/Animation/Pipeline/AnimationForwardSurfaceScope.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"

namespace Ken4lowEngine
{
	inline MaterialBlendMode AnimatedModelComponent::ResolveForwardBlendMode() const
	{
		return AnimationForwardSurface::ResolveBlendMode(materialBinding_);
	}

	inline bool AnimatedModelComponent::SubmitForwardBucket(ForwardRenderQueue& queue, MaterialBlendMode expectedBlendMode)
	{
		if (!visible_ || !IsActiveInHierarchy() || !animatedModel_ || !hasMesh_ || ResolveForwardBlendMode() != expectedBlendMode)
		{
			return false;
		}

		ForwardRenderItem item = MakeForwardRenderItem(
			this,
			[](void* payload)
			{
				auto* component = static_cast<AnimatedModelComponent*>(payload);
				const AnimationForwardSurface::ScopedBlendMode blendScope(component->ResolveForwardBlendMode());
				component->Draw(); // Actor通常Drawを飛ばし、Queueが所有する区間で既存AnimationModel描画を再利用する。
			},
			expectedBlendMode,
			AnimationForwardSurface::CalculateSortDepth(GetWorldPosition()));
		return queue.Submit(item);
	}

	inline bool AnimatedModelComponent::SubmitForwardOpaque(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Opaque);
	}

	inline bool AnimatedModelComponent::SubmitForwardMasked(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Masked);
	}

	inline bool AnimatedModelComponent::SubmitForwardTransparent(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Transparent);
	}

	inline bool AnimatedModelComponent::SubmitForwardAdditive(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Additive);
	}
} // namespace Ken4lowEngine
