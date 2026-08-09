#pragma once

#include "Camera.h"
#include "LinearInterpolation.h"

#include <algorithm>
#include <cmath>

namespace TitleCameraUtility
{
	/// 注視点を向くための水平角と上下角を一貫した計算で求める。
	inline void CalculateLookAtAngles(const Ken4lowEngine::Vector3& from, const Ken4lowEngine::Vector3& to, float& outYaw, float& outPitch)
	{
		const float dx = to.x - from.x;
		const float dy = to.y - from.y;
		const float dz = to.z - from.z;
		outYaw = std::atan2(dx, dz);
		outPitch = std::atan2(dy, std::sqrt(dx * dx + dz * dz));
	}

	/// タイトルとロビー間で共通するカメラ姿勢補間を更新し、正規化済み進捗率を返す。
	template<class OrbitState, class Timers, class Pose>
	float UpdatePoseTransition(OrbitState& orbitState, Timers& timers, const Pose& poseFrom, const Pose& poseTo, Ken4lowEngine::Camera& camera, float deltaTime)
	{
		timers.time += deltaTime;
		const float progress = std::clamp(timers.time / (std::max)(timers.duration, 0.0001f), 0.0f, 1.0f);
		const float eased = Ken4lowEngine::EaseInOutCubic(progress);
		const Ken4lowEngine::Vector3 position{
			Ken4lowEngine::Lerp(poseFrom.position.x, poseTo.position.x, eased),
			Ken4lowEngine::Lerp(poseFrom.position.y, poseTo.position.y, eased),
			Ken4lowEngine::Lerp(poseFrom.position.z, poseTo.position.z, eased),
		};
		const float yaw = Ken4lowEngine::LerpAngle(poseFrom.yaw, poseTo.yaw, eased);
		const float pitch = Ken4lowEngine::Lerp(poseFrom.pitch, poseTo.pitch, eased);
		camera.SetTranslate(position);
		camera.SetRotate({ pitch, yaw, 0.0f });
		camera.Update();
		orbitState.lastYaw = yaw;
		orbitState.lastPitch = pitch;
		return progress;
	}
}
