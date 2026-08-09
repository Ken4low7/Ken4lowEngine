#define NOMINMAX
#include "TitleLobbyState.h"
#include "TitleScene.h"
#include "TitleAttractState.h"
#include "TitleLobbyToTitleState.h"
#include "TitleLoadState.h"
#include "TitleCameraUtility.h"
#include "SceneManager.h"
#include "Input.h"
#include <LinearInterpolation.h>
#include <algorithm>

using namespace Ken4lowEngine;

void TitleLobbyState::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::LobbyIdle);

	// ロビーに入った瞬間は押下・ホバー状態を初期化し、ボタン表示が不自然に出ないようにする。
	auto& battleButtonUI = scene->GetBattleButtonUI();
	battleButtonUI.isPressing = false;
	battleButtonUI.pressAnim = 0.0f;
	battleButtonUI.hoverAnim = 0.0f;
	battleButtonUI.appearAnim = 0.0f;
}

void TitleLobbyState::Update(TitleScene* scene, float deltaTime)
{
	using State = TitleScene::State;
	State state = scene->GetState();

	auto& orbitState = scene->GetOrbitState();
	auto& logoUI = scene->GetLogoUI();
	auto& timers = scene->GetTimers();
	auto& lobbySwing = scene->GetLobbySwing();
	auto& poseFrom = scene->GetPoseFrom();
	auto& poseTo = scene->GetPoseTo();
	auto& battleButtonUI = scene->GetBattleButtonUI();
	auto sceneManager = scene->GetSceneManager();

	Camera* camera = scene->GetCamera();
	Input* input = scene->GetInput();

	Vector2 mp = input->GetMousePosition();

	// ボタン矩形（アンカー対応）
	const float minX = battleButtonUI.position.x - battleButtonUI.size.x * battleButtonUI.anchor.x;
	const float minY = battleButtonUI.position.y - battleButtonUI.size.y * battleButtonUI.anchor.y;
	const float maxX = minX + battleButtonUI.size.x;
	const float maxY = minY + battleButtonUI.size.y;
	const bool inBtn = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

	// ロビーに入った直後はボタンを下からフェードインさせ、急にポンと出ないようにする。
	battleButtonUI.appearAnim = std::min(1.0f, battleButtonUI.appearAnim + deltaTime * 4.0f);
	const float appearEase = 1.0f - ((1.0f - battleButtonUI.appearAnim) * (1.0f - battleButtonUI.appearAnim) * (1.0f - battleButtonUI.appearAnim));
	const bool canDecideButton = battleButtonUI.appearAnim >= 0.85f;

	// ----- クリック確定ルール -----
	if (canDecideButton && input->TriggerMouse(0) && inBtn)
	{
		battleButtonUI.isPressing = true;
	}

	const bool mouseHeld = input->PushMouse(0);
	const bool mouseUp = input->ReleaseMouse(0);

	if (mouseUp)
	{
		if (canDecideButton && battleButtonUI.isPressing && inBtn)
		{
			battleButtonUI.isPressing = false;

			// 即シーン変更要求
			sceneManager->ChangeScene("StageSelectScene");

			return;
		}

		battleButtonUI.isPressing = false;
	}

	// ReleaseでもImGuiボタンに依存せず、キーボード/ゲームパッドでBattleボタンを決定できるようにする。
	if (canDecideButton && (input->TriggerKey(DIK_RETURN) || input->TriggerKey(DIK_SPACE) || input->TriggerButton(XButtons.A)))
	{
		sceneManager->ChangeScene("StageSelectScene");
		return;
	}

	// ----- 視覚効果 -----
	const float pressTarget = (canDecideButton && battleButtonUI.isPressing && mouseHeld) ? 1.0f : 0.0f;
	const float hoverTarget = (canDecideButton && !pressTarget && inBtn) ? 1.0f : 0.0f;

	const float s = std::clamp(deltaTime * 12.0f, 0.0f, 1.0f);
	battleButtonUI.pressAnim = Lerp(battleButtonUI.pressAnim, pressTarget, s);
	battleButtonUI.hoverAnim = Lerp(battleButtonUI.hoverAnim, hoverTarget, s);

	const float buttonScale =
		(1.0f + battleButtonUI.scaleHover * battleButtonUI.hoverAnim) - (battleButtonUI.scalePress * battleButtonUI.pressAnim);
	const float appearScale = 0.72f + 0.28f * appearEase;
	const float scale = buttonScale * appearScale;

	const Vector2 pos = {
		battleButtonUI.position.x,
		battleButtonUI.position.y + battleButtonUI.pressOffsetPx * battleButtonUI.pressAnim + battleButtonUI.appearOffsetPx * (1.0f - appearEase)
	};

	const float tint = 1.0f - 0.15f * battleButtonUI.pressAnim;
	const float alpha = appearEase;

	if (battleButtonUI.btnShadow)
	{
		const float shadowOffset = Lerp(8.0f, 3.0f, battleButtonUI.pressAnim);
		battleButtonUI.btnShadow->SetSize({ battleButtonUI.size.x * (scale + 0.03f), battleButtonUI.size.y * (scale + 0.03f) });
		battleButtonUI.btnShadow->SetPosition({ battleButtonUI.position.x, pos.y + shadowOffset });
		battleButtonUI.btnShadow->SetColor({ 0, 0, 0, (0.32f + 0.10f * battleButtonUI.hoverAnim) * alpha });
		battleButtonUI.btnShadow->Update();
	}

	if (battleButtonUI.btnBorder)
	{
		battleButtonUI.btnBorder->SetSize({ battleButtonUI.size.x * scale + 10.0f, battleButtonUI.size.y * scale + 10.0f });
		battleButtonUI.btnBorder->SetPosition(pos);
		battleButtonUI.btnBorder->SetColor({ 0.10f, 0.35f + 0.18f * battleButtonUI.hoverAnim, 0.20f, 0.96f * alpha });
		battleButtonUI.btnBorder->Update();
	}

	if (battleButtonUI.btnSprite)
	{
		battleButtonUI.btnSprite->SetSize({ battleButtonUI.size.x * scale, battleButtonUI.size.y * scale });
		battleButtonUI.btnSprite->SetPosition(pos);
		battleButtonUI.btnSprite->SetColor({ 0.36f * tint, 0.92f * tint, 0.52f * tint, 0.94f * alpha });
		battleButtonUI.btnSprite->Update();
	}

	const float accentOffsetX = battleButtonUI.size.x * 0.5f * scale - 28.0f;
	const float accentW = 16.0f + 5.0f * battleButtonUI.hoverAnim;
	const float accentH = battleButtonUI.size.y * scale - 34.0f;
	if (battleButtonUI.btnAccentLeft)
	{
		battleButtonUI.btnAccentLeft->SetPosition({ pos.x - accentOffsetX, pos.y });
		battleButtonUI.btnAccentLeft->SetSize({ accentW, accentH });
		battleButtonUI.btnAccentLeft->SetColor({ 0.92f, 1.0f, 0.70f, (0.92f + 0.08f * battleButtonUI.hoverAnim) * alpha });
		battleButtonUI.btnAccentLeft->Update();
	}
	if (battleButtonUI.btnAccentRight)
	{
		battleButtonUI.btnAccentRight->SetPosition({ pos.x + accentOffsetX, pos.y });
		battleButtonUI.btnAccentRight->SetSize({ accentW, accentH });
		battleButtonUI.btnAccentRight->SetColor({ 0.92f, 1.0f, 0.70f, (0.92f + 0.08f * battleButtonUI.hoverAnim) * alpha });
		battleButtonUI.btnAccentRight->Update();
	}

	// 無操作タイマー更新
	timers.idle += deltaTime;
	if (input->TriggerMouse(0))
	{
		timers.idle = 0.0f;
	}

	// ロビーでは注視点を保ったまま小さく左右へ揺らし、待機中の画面に動きを付ける。
	if (camera)
	{
		lobbySwing.phase += deltaTime * lobbySwing.speed;
		const float theta = lobbySwing.baseTheta + std::sin(lobbySwing.phase) * lobbySwing.amplitude;

		const float x = lobbySwing.lookAt.x + lobbySwing.radius * std::sin(theta);
		const float z = lobbySwing.lookAt.z + lobbySwing.radius * std::cos(theta);
		const float y = lobbySwing.height;
		camera->SetTranslate({ x, y, z });

		const float yaw = theta + std::numbers::pi_v<float>;
		camera->SetRotate({ lobbySwing.basePitch, yaw, 0.0f });
		camera->Update();
		orbitState.lastYaw = yaw; orbitState.lastPitch = lobbySwing.basePitch;
	}

	// 一定時間入力がなければ、現在姿勢を始点としてタイトル表示へ滑らかに戻す。
	if (timers.idle >= timers.returnSeconds && camera)
	{
		Vector3 orbitPos{
			orbitState.center.x + orbitState.radius * std::sin(orbitState.angle),
			orbitState.center.y,
			orbitState.center.z + orbitState.radius * std::cos(orbitState.angle)
		};
		float toYaw = 0.0f, toPitch = 0.0f;
		TitleCameraUtility::CalculateLookAtAngles(orbitPos, orbitState.center, toYaw, toPitch);

		poseFrom = { camera->GetTranslate(), orbitState.lastYaw, orbitState.lastPitch };
		poseTo = { orbitPos, toYaw, toPitch };
		timers.time = 0.0f;
		timers.state = 0.0f;
		timers.inputCooldownLeft = timers.afterReturnCooldown;
		logoUI.showLeft = logoUI.showDelay;

		scene->SetState(State::ToTitle);
		if (state == State::ToTitle) { scene->ChangeState(std::make_unique<TitleLobbyToTitleState>()); }
		return;
	}
}
void TitleLobbyState::Exit(TitleScene* scene)
{
	(void)scene;
}
