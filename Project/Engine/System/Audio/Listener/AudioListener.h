#pragma once

#include "Vector3.h"

namespace Ken4lowEngine
{
	class AudioListener
	{
	public:
		void UpdateFromTransform(const Vector3& position, const Vector3& forward, const Vector3& right, const Vector3& up, float deltaTime);

		const Vector3& GetPosition() const { return position_; }
		const Vector3& GetPreviousPosition() const { return previousPosition_; }
		const Vector3& GetVelocity() const { return velocity_; }
		const Vector3& GetForward() const { return forward_; }
		const Vector3& GetRight() const { return right_; }
		const Vector3& GetUp() const { return up_; }

	private:
		Vector3 position_{};
		Vector3 previousPosition_{};
		Vector3 velocity_{};
		Vector3 forward_{ 0.0f, 0.0f, 1.0f };
		Vector3 right_{ 1.0f, 0.0f, 0.0f };
		Vector3 up_{ 0.0f, 1.0f, 0.0f };
	};
}
