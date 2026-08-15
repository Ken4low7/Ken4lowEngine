#pragma once

#include "AnimationPipelineBuilder.h"
#include "CameraManager.h"
#include "Engine/Graphics/Material/MaterialBinding.h"

namespace Ken4lowEngine::AnimationForwardSurface
{
	inline MaterialBlendMode ResolveBlendMode(const MaterialBinding& binding)
	{
		if (!binding.HasBinding()) return MaterialBlendMode::Opaque;
		MaterialDesc desc{};
		return binding.Resolve(desc) ? desc.blendMode : MaterialBlendMode::Opaque;
	}

	inline float CalculateSortDepth(const Vector3& worldPosition)
	{
		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const Vector3 cameraForward = CameraManager::GetInstance()->GetActiveCameraForward();
		const Vector3 toObject = {
			worldPosition.x - cameraPosition.x,
			worldPosition.y - cameraPosition.y,
			worldPosition.z - cameraPosition.z,
		};
		return toObject.x * cameraForward.x + toObject.y * cameraForward.y + toObject.z * cameraForward.z;
	}

	class ScopedBlendMode
	{
	public:
		explicit ScopedBlendMode(MaterialBlendMode blendMode)
			: builder_(AnimationPipelineBuilder::GetInstance()), previousBlendMode_(builder_->GetSurfaceBlendMode())
		{
			builder_->SetSurfaceBlendMode(blendMode); // Queue itemの実行区間だけAnimated PSO分類を切り替える。
		}

		~ScopedBlendMode()
		{
			builder_->SetSurfaceBlendMode(previousBlendMode_);
		}

		ScopedBlendMode(const ScopedBlendMode&) = delete;
		ScopedBlendMode& operator=(const ScopedBlendMode&) = delete;

	private:
		AnimationPipelineBuilder* builder_ = nullptr;
		MaterialBlendMode previousBlendMode_ = MaterialBlendMode::Opaque;
	};
} // namespace Ken4lowEngine::AnimationForwardSurface
