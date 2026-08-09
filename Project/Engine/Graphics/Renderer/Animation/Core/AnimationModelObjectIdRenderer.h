#pragma once

#include "AnimationModel.h"

#include <cstdint>

namespace Ken4lowEngine
{
	/// <summary>
	/// AnimationModelが公開するMeshとWorldTransformを使い、Editor Object-ID Passへ形状を描画します。
	/// </summary>
	void DrawAnimationModelObjectId(AnimationModel& animationModel, uint32_t objectId);
	
} // namespace Ken4lowEngine
