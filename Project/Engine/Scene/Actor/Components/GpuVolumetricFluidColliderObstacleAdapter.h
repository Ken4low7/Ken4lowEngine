#pragma once

#include "../../../Graphics/Renderer/GpuFluid/Volumetric/Data/GpuVolumetricFluidObstacleTypes.h"

#include <vector>

namespace Ken4lowEngine
{

class ActorWorld;
class ColliderComponent;

/// Physics Colliderを3D Volumetric Fluid用Obstacle Sourceへ変換する薄いAdapter。
class GpuVolumetricFluidColliderObstacleAdapter final
{
public:
	static bool BuildSource(
		const ColliderComponent& component,
		GpuVolumetricFluidObstacleSource& outSource);

	static void CollectSources(
		const ActorWorld& world,
		std::vector<GpuVolumetricFluidObstacleSource>& outSources);
};

} // namespace Ken4lowEngine
