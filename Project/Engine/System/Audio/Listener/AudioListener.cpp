#include "AudioListener.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	namespace
	{
		Vector3 ClampVelocity(const Vector3& velocity)
		{
			constexpr float kMaxVelocity = 10000.0f;
			return {
				std::clamp(velocity.x, -kMaxVelocity, kMaxVelocity),
				std::clamp(velocity.y, -kMaxVelocity, kMaxVelocity),
				std::clamp(velocity.z, -kMaxVelocity, kMaxVelocity)
			};
		}
	}

	void AudioListener::UpdateFromTransform(const Vector3& position, const Vector3& forward, const Vector3& right, const Vector3& up, float deltaTime)
	{
		previousPosition_ = position_;
		position_ = position;

		if (deltaTime > 0.0f && std::isfinite(deltaTime))
		{
			velocity_ = ClampVelocity((position_ - previousPosition_) / deltaTime);
		}

		forward_ = Vector3::NormalizeSafe(forward, forward_);
		right_ = Vector3::NormalizeSafe(right, right_);
		up_ = Vector3::NormalizeSafe(up, up_);
	}
}
