#pragma once
#include "AABB.h"
#include "BoundingVolume.h"
#include "Vector4.h"

#include <string>

namespace Ken4lowEngine
{
	/// <summary>
	/// CPU Occlusion Culling で遮蔽物として扱う簡易 AABB。
	/// </summary>
	class Occluder
	{
	public:
		Occluder() = default;
		Occluder(const BoundingAABB& worldBounds, const std::string& debugName = "");

		const BoundingAABB& GetWorldBounds() const { return worldBounds_; }
		AABB GetDebugAABB() const;
		const std::string& GetDebugName() const { return debugName_; }
		bool IsEnabled() const { return enabled_; }
		void SetEnabled(bool enabled) { enabled_ = enabled; }

	private:
		BoundingAABB worldBounds_{};
		std::string debugName_{};
		bool enabled_ = true;
	};
}
