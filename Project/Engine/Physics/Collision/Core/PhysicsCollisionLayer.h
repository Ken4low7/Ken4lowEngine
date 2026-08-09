#pragma once
#include <cstdint>

namespace Ken4lowEngine
{
	// PhysicsWorldで使用する基本衝突レイヤーを名前で扱うための定義
	enum class PhysicsCollisionLayer : uint32_t
	{
		Default = 0,
		DynamicActor = 1,
		WorldStatic = 2,

		Count
	};
}