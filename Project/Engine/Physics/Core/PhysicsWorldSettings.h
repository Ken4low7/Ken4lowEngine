#pragma once

#include "Vector3.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	/// PhysicsWorldへ外部から反映する調整値
	/// -------------------------------------------------------------
	struct PhysicsWorldSettings
	{
		bool useFixedStep = true;
		float fixedTimeStep = 1.0f / 60.0f;
		float maxDeltaTime = 0.1f;
		int maxSubSteps = 4;
		Vector3 gravity{ 0.0f, -9.8f, 0.0f };
		bool enablePositionSolver = true;
		bool enableVelocitySolver = true;
		bool enableFrictionSolver = true;
		bool enableSleep = true;
	};

} // namespace Ken4lowEngine
