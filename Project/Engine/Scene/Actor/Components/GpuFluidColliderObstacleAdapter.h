#pragma once

#include "../../../Graphics/Renderer/GpuFluid/Data/GpuFluidObstacleTypes.h"

#include <vector>

namespace Ken4lowEngine
{

class ActorWorld;
class ColliderComponent;

/// Actor/Physics側のColliderをGPU Fluid用Obstacle Sourceへ変換する薄いAdapter。
class GpuFluidColliderObstacleAdapter final
{
public:
	static bool BuildSource(
		const ColliderComponent& component,
		GpuFluidObstacleSource& outSource);

	static void CollectSources(
		const ActorWorld& world,
		std::vector<GpuFluidObstacleSource>& outSources);
};

} // namespace Ken4lowEngine
