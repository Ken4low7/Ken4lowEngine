#include "TitleLobbyToTitleState.h"
#include "TitleScene.h"
#include <LinearInterpolation.h>
#include "TitleAttractState.h"
#include "TitleCameraUtility.h"

using namespace Ken4lowEngine;

void TitleLobbyToTitleState::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::ToTitle);
}

void TitleLobbyToTitleState::Update(TitleScene* scene, float deltaTime)
{
	// 今のシーン状態を取得
	using State = TitleScene::State;
	State state = scene->GetState();

	// TitleScene 内部状態への参照／ポインタを取得
	auto& orbitState = scene->GetOrbitState();
	auto& logoUI = scene->GetLogoUI();
	auto& timers = scene->GetTimers();
	auto& poseFrom = scene->GetPoseFrom();
	auto& poseTo = scene->GetPoseTo();

	Camera* camera = scene->GetCamera();

	const float t = TitleCameraUtility::UpdatePoseTransition(orbitState, timers, poseFrom, poseTo, *camera, deltaTime);

	// 補間完了→タイトルのオービットへ
	if (t >= 1.0f)
	{
		timers.idle = timers.state = 0.0f;
		scene->SetState(State::TitleAttract); // タイトルへ遷移

		logoUI.alpha = 0.0f;            // 戻ってもしばらく見えない
		logoUI.scale = 0.9f;
	}

	if (state == State::TitleAttract)
	{
		// ここでステートクラス自体を Lobby にバトンタッチ
		scene->ChangeState(std::make_unique<TitleAttractState>());
	}
}

void TitleLobbyToTitleState::Exit(TitleScene* scene)
{
	(void)scene;
}
