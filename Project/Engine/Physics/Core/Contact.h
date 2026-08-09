#pragma once
#include "Vector3.h"

namespace Ken4lowEngine
{
	class Collider;

	/// -------------------------------------------------------------
	///                         接触情報
	/// -------------------------------------------------------------
	struct Contact
	{
		Collider* colliderA = nullptr;
		Collider* colliderB = nullptr;
		Vector3 point{};
		Vector3 normal{};
		float penetration = 0.0f;
		bool isTrigger = false;
	};

} // namespace Ken4lowEngine
