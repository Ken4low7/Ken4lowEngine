#include "InputSnapshot.h"

#include <cmath>

namespace Ken4lowEngine
{
	void NormalizeClamp2(float& x, float& z)
	{
		const float lenSq = x * x + z * z;
		if (lenSq <= 1e-6f)
		{
			x = 0.0f;
			z = 0.0f;
			return;
		}

		const float len = std::sqrt(lenSq);
		x /= len;
		z /= len;
	}
} // namespace Ken4lowEngine