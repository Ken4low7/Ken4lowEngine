#define NOMINMAX
#include "GamePlayIntroDirector.h"

#include "GamePlayFlow.h"
#include "GamePlayWorld.h"

#include <Input.h>
#include "CameraManager.h"
#include "Player.h"

#include <algorithm>
#include <cmath>
#include <numbers>

using namespace Ken4lowEngine;

namespace
{
	constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
	constexpr float kPi = std::numbers::pi_v<float>;

	K4E::Vector3 DegToRadVec(const K4E::Vector3& deg)
	{
		return deg * kDegToRad;
	}

	K4E::Vector3 LerpVec3(const K4E::Vector3& a, const K4E::Vector3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	}

	float LerpFloat(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	float CatmullRomFloat(float p0, float p1, float p2, float p3, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return 0.5f * (
			(2.0f * p1) +
			(-p0 + p2) * t +
			(2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
			(-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3
			);
	}

	K4E::Vector3 CatmullRomVec3(const K4E::Vector3& p0, const K4E::Vector3& p1, const K4E::Vector3& p2, const K4E::Vector3& p3, float t)
	{
		return {
			CatmullRomFloat(p0.x, p1.x, p2.x, p3.x, t),
			CatmullRomFloat(p0.y, p1.y, p2.y, p3.y, t),
			CatmullRomFloat(p0.z, p1.z, p2.z, p3.z, t)
		};
	}

	const GamePlayIntroDirector::IntroCameraPointInfo& GetIntroPointClamped(
		const std::vector<GamePlayIntroDirector::IntroCameraPointInfo>& points,
		int index)
	{
		const int clamped = std::clamp(index, 0, static_cast<int>(points.size()) - 1);
		return points[static_cast<size_t>(clamped)];
	}

	K4E::Vector3 FindLookAtPosition(
		const std::vector<GamePlayIntroDirector::IntroLookAtPointInfo>& lookPoints,
		const std::string& name,
		const K4E::Vector3& fallback)
	{
		if (name.empty())
		{
			return fallback;
		}

		for (const auto& p : lookPoints)
		{
			if (p.name == name)
			{
				return p.position;
			}
		}

		return fallback;
	}

	K4E::Vector3 EulerDegToForward(const K4E::Vector3& rotDeg)
	{
		const K4E::Vector3 r = DegToRadVec(rotDeg);

		const float cx = std::cos(r.x);
		const float sx = std::sin(r.x);
		const float cy = std::cos(r.y);
		const float sy = std::sin(r.y);

		K4E::Vector3 forward{};
		forward.x = sy * cx;
		forward.y = -sx;
		forward.z = cy * cx;

		return K4E::Vector3::Normalize(forward);
	}

	K4E::Vector3 BuildAimTargetFromPoint(
		const GamePlayIntroDirector::IntroCameraPointInfo& point,
		const std::vector<GamePlayIntroDirector::IntroLookAtPointInfo>& lookPoints)
	{
		if (point.aimMode == "Euler")
		{
			return point.position + EulerDegToForward(point.rotation);
		}

		return FindLookAtPosition(
			lookPoints,
			point.targetName,
			point.position + K4E::Vector3{ 0.0f, 0.0f, 1.0f });
	}

	K4E::Vector3 EvaluateIntroPosition(
		const std::vector<GamePlayIntroDirector::IntroCameraPointInfo>& points,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			return CatmullRomVec3(p0.position, p1.position, p2.position, p3.position, t);
		}

		return LerpVec3(p1.position, p2.position, t);
	}

	K4E::Vector3 EvaluateIntroTarget(
		const std::vector<GamePlayIntroDirector::IntroCameraPointInfo>& points,
		const std::vector<GamePlayIntroDirector::IntroLookAtPointInfo>& lookPoints,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		const K4E::Vector3 a0 = BuildAimTargetFromPoint(p0, lookPoints);
		const K4E::Vector3 a1 = BuildAimTargetFromPoint(p1, lookPoints);
		const K4E::Vector3 a2 = BuildAimTargetFromPoint(p2, lookPoints);
		const K4E::Vector3 a3 = BuildAimTargetFromPoint(p3, lookPoints);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			return CatmullRomVec3(a0, a1, a2, a3, t);
		}

		return LerpVec3(a1, a2, t);
	}

	float EvaluateIntroFov(
		const std::vector<GamePlayIntroDirector::IntroCameraPointInfo>& points,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			return CatmullRomFloat(p0.fov, p1.fov, p2.fov, p3.fov, t);
		}

		return LerpFloat(p1.fov, p2.fov, t);
	}

	float WrapAngleRad(float a)
	{
		while (a > kPi) { a -= 2.0f * kPi; }
		while (a < -kPi) { a += 2.0f * kPi; }
		return a;
	}

	float LerpAngleRad(float a, float b, float t)
	{
		const float diff = WrapAngleRad(b - a);
		return WrapAngleRad(a + diff * t);
	}

	K4E::Vector3 LerpEulerRad(const K4E::Vector3& a, const K4E::Vector3& b, float t)
	{
		return {
			LerpAngleRad(a.x, b.x, t),
			LerpAngleRad(a.y, b.y, t),
			LerpAngleRad(a.z, b.z, t)
		};
	}

	K4E::Vector3 DirectionToEulerRad(const K4E::Vector3& dir)
	{
		K4E::Vector3 n = K4E::Vector3::Normalize(dir);

		const float yaw = std::atan2(-n.x, n.z);
		const float horizontalLen = std::sqrt(n.x * n.x + n.z * n.z);
		const float pitch = std::atan2(-n.y, horizontalLen);

		return { pitch, yaw, 0.0f };
	}

	K4E::Vector3 LookAtToEulerRad(const K4E::Vector3& from, const K4E::Vector3& to)
	{
		return DirectionToEulerRad(to - from);
	}

	K4E::Vector3 EvaluateIntroEulerRotation(
		const std::vector<GamePlayIntroDirector::IntroCameraPointInfo>& points,
		int segmentIndex,
		float t)
	{
		const auto& p0 = GetIntroPointClamped(points, segmentIndex - 1);
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);
		const auto& p3 = GetIntroPointClamped(points, segmentIndex + 2);

		const K4E::Vector3 r0 = DegToRadVec(p0.rotation);
		const K4E::Vector3 r1 = DegToRadVec(p1.rotation);
		const K4E::Vector3 r2 = DegToRadVec(p2.rotation);
		const K4E::Vector3 r3 = DegToRadVec(p3.rotation);

		if (p1.interpMode == "CatmullRom" && points.size() >= 2)
		{
			K4E::Vector3 rot = CatmullRomVec3(r0, r1, r2, r3, t);
			rot.x = WrapAngleRad(rot.x);
			rot.y = WrapAngleRad(rot.y);
			rot.z = WrapAngleRad(rot.z);
			return rot;
		}

		return LerpEulerRad(r1, r2, t);
	}

	K4E::Vector3 EvaluateIntroCameraRotation(
		const std::vector<GamePlayIntroDirector::IntroCameraPointInfo>& points,
		const std::vector<GamePlayIntroDirector::IntroLookAtPointInfo>& lookPoints,
		int segmentIndex,
		float t,
		const K4E::Vector3& camPos)
	{
		const auto& p1 = GetIntroPointClamped(points, segmentIndex);
		const auto& p2 = GetIntroPointClamped(points, segmentIndex + 1);

		if (p1.aimMode == "Euler" || p2.aimMode == "Euler")
		{
			return EvaluateIntroEulerRotation(points, segmentIndex, t);
		}

		const K4E::Vector3 lookPos = EvaluateIntroTarget(points, lookPoints, segmentIndex, t);
		return LookAtToEulerRad(camPos, lookPos);
	}

	void ApplyCameraPoint(
		const GamePlayIntroDirector::IntroCameraPointInfo& point,
		const std::vector<GamePlayIntroDirector::IntroLookAtPointInfo>& lookPoints,
		K4E::Camera& camera)
	{
		const K4E::Vector3 rotation = point.aimMode == "Euler"
			? DegToRadVec(point.rotation)
			: LookAtToEulerRad(point.position, FindLookAtPosition(
				lookPoints, point.targetName, point.position + K4E::Vector3{ 0.0f, 0.0f, 1.0f }));
		camera.SetTranslate(point.position);
		camera.SetRotate(rotation);
		camera.SetFovY(point.fov * kDegToRad);
		camera.Update();
	}
}

void GamePlayIntroDirector::Reset(const GamePlayStageContext& stageContext, float introDuration)
{
	cameraPoints_ = stageContext.GetIntroCameraPoints();
	lookAtPoints_ = stageContext.GetIntroLookAtPoints();

	introDuration_ = introDuration;
	introTimer_ = introDuration_;
	currentSegment_ = 0;
	segmentTimer_ = 0.0f;
}

void GamePlayIntroDirector::Update(
	float deltaTime,
	GamePlayFlow& flow,
	const GamePlayStageContext& stageContext,
	GamePlayWorld& world,
	K4E::Input* input,
	bool isDebugCamera)
{
	if (flow.GetState() == GamePlayFlow::State::EquipIntro)
	{
		world.UpdateEquipIntro(deltaTime);
		if (auto* player = world.GetCharacters().GetPlayer())
		{
			if (!player->IsWeaponEquipAnimating())
			{
				flow.StartPlaying();
				world.StartWaves();
			}
		}
		return;
	}

	if (cameraPoints_.empty())
	{
		BeginGamePlayFromIntro(flow, stageContext, world, input, isDebugCamera);
		return;
	}

	world.UpdateIntroVisuals();

	K4E::Camera* camera = CameraManager::GetInstance()->GetMainCamera();
	if (!camera)
	{
		return;
	}

	if (input && input->TriggerKey(DIK_SPACE))
	{
		// Space入力時は最終カメラ位置へ合わせてからイントロを終了する。
		const auto& p = cameraPoints_.back();
		ApplyCameraPoint(p, lookAtPoints_, *camera);

		BeginGamePlayFromIntro(flow, stageContext, world, input, isDebugCamera);
		return;
	}

	if (cameraPoints_.size() == 1)
	{
		const auto& p = cameraPoints_[0];
		ApplyCameraPoint(p, lookAtPoints_, *camera);

		introTimer_ -= deltaTime;
		if (introTimer_ <= 0.0f)
		{
			BeginGamePlayFromIntro(flow, stageContext, world, input, isDebugCamera);
		}
		return;
	}

	if (currentSegment_ >= static_cast<int>(cameraPoints_.size()) - 1)
	{
		BeginGamePlayFromIntro(flow, stageContext, world, input, isDebugCamera);
		return;
	}

	const auto& from = cameraPoints_[static_cast<size_t>(currentSegment_)];
	const float segmentDuration = std::max(0.01f, from.duration);

	segmentTimer_ += deltaTime;

	float t = segmentTimer_ / segmentDuration;
	t = std::clamp(t, 0.0f, 1.0f);

	const K4E::Vector3 camPos = EvaluateIntroPosition(cameraPoints_, currentSegment_, t);
	const K4E::Vector3 camRot = EvaluateIntroCameraRotation(cameraPoints_, lookAtPoints_, currentSegment_, t, camPos);
	const float fovDeg = EvaluateIntroFov(cameraPoints_, currentSegment_, t);

	camera->SetTranslate(camPos);
	camera->SetRotate(camRot);
	camera->SetFovY(fovDeg * kDegToRad);
	camera->Update();

	if (t >= 1.0f)
	{
		++currentSegment_;
		segmentTimer_ = 0.0f;

		if (currentSegment_ >= static_cast<int>(cameraPoints_.size()) - 1)
		{
			BeginGamePlayFromIntro(flow, stageContext, world, input, isDebugCamera);
		}
	}
}

void GamePlayIntroDirector::BeginGamePlayFromIntro(
	GamePlayFlow& flow,
	const GamePlayStageContext& stageContext,
	GamePlayWorld& world,
	K4E::Input* input,
	bool isDebugCamera)
{
	if (flow.GetState() != GamePlayFlow::State::Intro)
	{
		return;
	}

	K4E::Vector3 introCameraRotation{};
	if (const auto* camera = K4E::CameraManager::GetInstance()->GetMainCamera())
	{
		introCameraRotation = camera->GetRotate();
	}

	flow.SetState(GamePlayFlow::State::EquipIntro);

	introTimer_ = 0.0f;
	currentSegment_ = 0;
	segmentTimer_ = 0.0f;

	if (auto* player = world.GetCharacters().GetPlayer())
	{
		player->SetSpawnOffset({ 0.0f, 0.0f, 0.0f });

		if (stageContext.HasPlayerSpawnPoint())
		{
			constexpr float kPlayerSpawnLift = 1.0f;

			K4E::Vector3 spawn = stageContext.GetPlayerSpawnPoint();
			spawn.y += kPlayerSpawnLift;
			player->SetSpawnPosition(spawn);
		}
	}

	world.SyncAfterPlayerSpawn();

	// カメラ切り替え時の一括初期化を避けるため、ここでは事前生成済みビジュアルの表示だけ戻す。
	world.SetStartGameplayVisualsVisible(true);

	if (auto* player = world.GetCharacters().GetPlayer())
	{
		// 完了フレーム内でFPSカメラの初期向きを同期し、次フレーム待ちの停止感と向きの跳ねを防ぐ。
		player->SetViewLookAngles(introCameraRotation.x, introCameraRotation.y);
		player->SyncViewToPlayer();
		player->StartWeaponEquipAnimation();
	}

	if (input)
	{
		const bool lock = !isDebugCamera;
		input->SetLockCursor(lock);
		input->SetCursorVisible(!lock);
	}
}
