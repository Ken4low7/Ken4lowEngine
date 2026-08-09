#pragma once
#include <cstdint>

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///                         物理種別
	/// -------------------------------------------------------------
	enum class BodyType : uint8_t
	{
		Static,
		Dynamic,
		Kinematic,
	};

	/// -------------------------------------------------------------
	///                         衝突モード
	/// -------------------------------------------------------------
	enum class CollisionMode : uint8_t
	{
		Collision,
		Trigger,
	};

	/// -------------------------------------------------------------
	///                         衝突応答種別
	/// -------------------------------------------------------------
	enum class CollisionResponseType : uint8_t
	{
		Ignore,
		Trigger,
		Block,
	};

} // namespace Ken4lowEngine
