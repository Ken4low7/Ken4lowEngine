#pragma once

#include "Engine/Graphics/Renderer/Animation/Pipeline/AnimationForwardSurfaceScope.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"

namespace Ken4lowEngine
{
	inline MaterialBlendMode SkeletalMeshComponent::ResolveForwardBlendMode() const
	{
		return AnimationForwardSurface::ResolveBlendMode(materialBinding_);
	}

	inline bool SkeletalMeshComponent::SubmitForwardBucket(ForwardRenderQueue& queue, MaterialBlendMode expectedBlendMode)
	{
		if (!visible_ || !IsActiveInHierarchy() || !animationModel_ || !hasMesh_ || ResolveForwardBlendMode() != expectedBlendMode)
		{
			return false;
		}

		ForwardRenderItem item = MakeForwardRenderItem(
			this,
			[](void* payload)
			{
				auto* component = static_cast<SkeletalMeshComponent*>(payload);
				const AnimationForwardSurface::ScopedBlendMode blendScope(component->ResolveForwardBlendMode());
				component->Draw(); // Compute Skinningを含む既存DrawをForward Bucket実行時にそのまま再利用する。
			},
			expectedBlendMode,
			AnimationForwardSurface::CalculateSortDepth(GetWorldPosition()));
		return queue.Submit(item);
	}

	inline bool SkeletalMeshComponent::SubmitForwardOpaque(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Opaque);
	}

	inline bool SkeletalMeshComponent::SubmitForwardMasked(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Masked);
	}

	inline bool SkeletalMeshComponent::SubmitForwardTransparent(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Transparent);
	}

	inline bool SkeletalMeshComponent::SubmitForwardAdditive(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Additive);
	}
} // namespace Ken4lowEngine
