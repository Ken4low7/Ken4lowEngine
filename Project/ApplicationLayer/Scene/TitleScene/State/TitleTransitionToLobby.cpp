#define NOMINMAX
#include "TitleTransitionToLobby.h"
#include "TitleScene.h"
#include "TitleLobbyState.h"
#include "TitleCameraUtility.h"
#include <LinearInterpolation.h>

using namespace Ken4lowEngine;

void TitleTransitionToLobby::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::TransitionToLobby);
}

void TitleTransitionToLobby::Update(TitleScene* scene, float deltaTime)
{
	// 今のシーン状態を取得
	using State = TitleScene::State;
	State state = scene->GetState();

	// TitleScene 内部状態への参照／ポインタを取得
	auto& orbitState = scene->GetOrbitState();
	auto& logoUI = scene->GetLogoUI();
	auto& timers = scene->GetTimers();
	auto& lobbySwing = scene->GetLobbySwing();
	auto& poseFrom = scene->GetPoseFrom();
	auto& poseTo = scene->GetPoseTo();
	Camera* camera = scene->GetCamera();

	const float t = TitleCameraUtility::UpdatePoseTransition(orbitState, timers, poseFrom, poseTo, *camera, deltaTime);

	if (t >= 1.0f)
	{
		scene->SetState(State::LobbyIdle);
		timers.state = timers.idle = 0.0f;

		// lookAt からの水平半径と高さ
		const Vector3& P = poseTo.position;
		lobbySwing.radius = std::hypot(P.x - lobbySwing.lookAt.x, P.z - lobbySwing.lookAt.z);
		lobbySwing.height = P.y;

		// 基準角（θ）とピッチを記録：yaw = θ + π なので θ = yaw - π
		lobbySwing.baseTheta = poseTo.yaw - std::numbers::pi_v<float>;
		lobbySwing.basePitch = poseTo.pitch;
		lobbySwing.phase = 0.0f;
	}

	// ロゴの退出フェード（あれば減衰）
	if (logoUI.exitLeft > 0.0f)
	{
		logoUI.exitLeft = std::max(0.0f, logoUI.exitLeft - deltaTime);
		logoUI.alpha = (logoUI.exitLeft / logoUI.exitFade);  // 線形でOK
	}

	if (state == State::LobbyIdle)
	{
		// ロビー待機へ移行
		scene->ChangeState(std::make_unique<TitleLobbyState>());
	}
}

void TitleTransitionToLobby::Exit(TitleScene* scene)
{
	(void)scene;
}
