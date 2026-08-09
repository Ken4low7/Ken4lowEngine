#include "Occluder.h"

namespace Ken4lowEngine
{
	Occluder::Occluder(const BoundingAABB& worldBounds, const std::string& debugName)
		: worldBounds_(worldBounds), debugName_(debugName)
	{
	}

	AABB Occluder::GetDebugAABB() const
	{
		AABB aabb{};
		aabb.min = worldBounds_.min;
		aabb.max = worldBounds_.max;
		return aabb;
	}
}
