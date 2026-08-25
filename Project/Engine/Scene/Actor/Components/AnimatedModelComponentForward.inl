#pragma once

#include "Engine/Graphics/Renderer/Animation/Pipeline/AnimationForwardSurfaceScope.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"

namespace Ken4lowEngine
{
	inline MaterialBlendMode AnimatedModelComponent::ResolveForwardBlendMode() const
	{
		return AnimationForwardSurface::ResolveBlendMode(materialBinding_);
	}

	inline void AnimatedModelComponent::DrawReflectionCapture()
	{
		if (!visible_ || !IsActiveInHierarchy() || !animatedModel_ || !hasMesh_) return;
		const MaterialBlendMode blendMode = ResolveForwardBlendMode();
		if (blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive) return;
		const AnimationForwardSurface::ScopedBlendMode blendScope(blendMode);
		Draw(); // Reflection Cameraへ切り替わった現在ViewでNode Animation姿勢を再描画する。
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
