#define NOMINMAX
#include "TitleAttractState.h"
#include "TitleScene.h"
#include <Camera.h>
#include "Input.h"
#include <LinearInterpolation.h>
#include <PostEffectManager.h>
#include "GameViewportConstants.h"
#include "TitleTransitionToLobby.h"
#include "TitleCameraUtility.h"

using namespace Ken4lowEngine;

void TitleAttractState::Enter(TitleScene* scene)
{
	scene->SetState(TitleScene::State::TitleAttract);
}

void TitleAttractState::Update(TitleScene* scene, float deltaTime)
{
	using State = TitleScene::State;
	State state = scene->GetState();

	auto& orbitState = scene->GetOrbitState();
	auto& logoUI = scene->GetLogoUI();
	auto& clickHintUI = scene->GetClickHintUI();
	auto& timers = scene->GetTimers();
	auto& lobbySwing = scene->GetLobbySwing();
	auto& poseFrom = scene->GetPoseFrom();
	auto& poseTo = scene->GetPoseTo();

	Camera* camera = scene->GetCamera();
	Input* input = scene->GetInput();
	Sprite* logoSprite = scene->GetLogoSprite();

	// ゆっくりカメラを周回させる
	if (camera)
	{
		// 角度を進める
		orbitState.angle += orbitState.speed * deltaTime;

		const float x = orbitState.center.x + orbitState.radius * sin(orbitState.angle);
		const float z = orbitState.center.z + orbitState.radius * cos(orbitState.angle);
		camera->SetTranslate({ x, orbitState.center.y, z });

		// 中心を見る
		TitleCameraUtility::CalculateLookAtAngles({ x, orbitState.center.y, z }, orbitState.center, orbitState.lastYaw, orbitState.lastPitch);
		camera->SetRotate({ orbitState.lastPitch, orbitState.lastYaw, 0.0f });
		camera->Update();
	}

	// ディレイ消化
	if (logoUI.showLeft > 0.0f) {
		logoUI.showLeft = std::max(0.0f, logoUI.showLeft - deltaTime);
	}

	// --- ロゴのフェード＆スケール（0.8秒でふわっと出す） ---
	{
		float t = (logoUI.showLeft > 0.0f) ? 0.0f
			: std::clamp((timers.state - logoUI.showDelay) / 0.8f, 0.0f, 1.0f);
		float te = EaseOutCubic(t);
		logoUI.alpha = te;
		logoUI.scale = 0.9f + 0.1f * te;
	}

	// 整数ピクセルにスナップ（ドット絵の滲み防止）
	auto snap = [](float v) { return std::floor(v + 0.5f); };

	bool canAcceptInput =
		(timers.state >= (timers.minTitleSeconds + logoUI.showDelay)) &&
		(timers.inputCooldownLeft <= 0.0f);

	clickHintUI.isVisible = canAcceptInput;   // 表示条件

	// === クリックヒント：追従・アニメ・ヒットテスト ===
	bool clickHintCommit = false;
	if (clickHintUI.isVisible && clickHintUI.hintSprite && logoSprite) {

		clickHintUI.phase += deltaTime;

		// ロゴのすぐ下（アンカー：ヒント=中央上）
		const Vector2 logoPos = logoSprite->GetPosition();
		const Vector2 logoSz = { logoUI.baseSize.x * logoUI.scale, logoUI.baseSize.y * logoUI.scale };
		Vector2 basePos = { logoPos.x, logoPos.y + (logoSz.y * 0.5f) + clickHintUI.marginY };

		// アニメ成分（点滅・上下ゆれ・脈動）
		const float blink = clickHintUI.blinkMin + (1.0f - clickHintUI.blinkMin) * (0.5f * (sinf(clickHintUI.phase * 2.2f) + 1.0f));
		const float wobble = sinf(clickHintUI.phase * 4.0f) * clickHintUI.wobblePx;
		const float pulse = 1.0f + clickHintUI.pulseMag * sinf(clickHintUI.phase * 2.0f);

		// Click表示を大きくして、タイトル画面で視認しやすく押しやすい導線にする。
		constexpr float kClickHintEmphasisScale = 2.25f;
		constexpr float kClickHitPaddingX = 48.0f;
		constexpr float kClickHitPaddingY = 24.0f;

		// いまの“見た目”でヒットテスト（前フレームの押し/ホバー値を反映）
		const float scaleNow =
			((pulse + clickHintUI.scaleHover * clickHintUI.hoverAnim) -
			(clickHintUI.scalePress * clickHintUI.pressAnim)) * kClickHintEmphasisScale;
		Vector2 posNow = { basePos.x, basePos.y + wobble + clickHintUI.offsetPressY * clickHintUI.pressAnim };
		posNow.y = std::min(posNow.y, static_cast<float>(GameViewportConstants::Height) - 120.0f); // クリック表示が大きくなっても画面下へはみ出さないようにする。

		const Vector2 sizeNow = { clickHintUI.baseSize.x * scaleNow, clickHintUI.baseSize.y * scaleNow };
		const float minX = posNow.x - sizeNow.x * 0.5f - kClickHitPaddingX; // アンカー(0.5,0.0)
		const float minY = posNow.y - kClickHitPaddingY;                    // 上端
		const float maxX = posNow.x + sizeNow.x * 0.5f + kClickHitPaddingX;
		const float maxY = posNow.y + sizeNow.y + kClickHitPaddingY;

		// マウス
		const Vector2 mp = input->GetMousePosition();
		const bool inHint = (mp.x >= minX && mp.x <= maxX && mp.y >= minY && mp.y <= maxY);

		// 入力：押し始めは内側、離したのも内側なら確定
		if (input->TriggerMouse(0) && inHint) clickHintUI.isPressing = true;
		const bool mouseHeld = input->PushMouse(0);
		const bool mouseUp = input->ReleaseMouse(0);
		if (mouseUp) {
			if (clickHintUI.isPressing && inHint) clickHintCommit = true; // ← 確定
			clickHintUI.isPressing = false;
		}

		// 目標値→アニメ補間
		const float pressTarget = (clickHintUI.isPressing && mouseHeld) ? 1.0f : 0.0f;
		const float hoverTarget = (!pressTarget && inHint) ? 1.0f : 0.0f;
		const float s = std::clamp(deltaTime * 12.0f, 0.0f, 1.0f);
		clickHintUI.pressAnim = Lerp(clickHintUI.pressAnim, pressTarget, s);
		clickHintUI.hoverAnim = Lerp(clickHintUI.hoverAnim, hoverTarget, s);

		//“更新後”の見た目で描画セット（次フレームの ③ で使われる）
		const float scaleDraw =
			((pulse + clickHintUI.scaleHover * clickHintUI.hoverAnim) -
			(clickHintUI.scalePress * clickHintUI.pressAnim)) * kClickHintEmphasisScale;
		Vector2 posDraw = {
			basePos.x,
			basePos.y + wobble + clickHintUI.offsetPressY * clickHintUI.pressAnim
		};
		posDraw.y = std::min(posDraw.y, static_cast<float>(GameViewportConstants::Height) - 120.0f);

		clickHintUI.hintSprite->SetPosition({ snap(posDraw.x), snap(posDraw.y) });
		clickHintUI.hintSprite->SetSize({ clickHintUI.baseSize.x * scaleDraw, clickHintUI.baseSize.y * scaleDraw });
		clickHintUI.hintSprite->SetColor({ 1.0f, 0.95f, 0.25f, std::max(0.75f, blink) });
		clickHintUI.hintSprite->Update();
	}

	// === 入力受付 ===
	const bool titleAdvanceInput = clickHintCommit ||
		input->TriggerKey(DIK_RETURN) ||
		input->TriggerKey(DIK_SPACE) ||
		input->TriggerButton(XButtons.A);
	// ReleaseでもImGuiボタンに依存せず、キーボード/マウス/ゲームパッドでTitleSceneを進行できるようにする。
	if (canAcceptInput && titleAdvanceInput) {

		// カメラ姿勢スナップショット
		Camera* cam = camera ? camera : scene->GetCamera();

		// 現在姿勢 -> ロビーの姿勢 へのスナップショットを取得
		poseFrom = { cam->GetTranslate(), orbitState.lastYaw, orbitState.lastPitch };
		float toYaw = 0.0f, toPitch = 0.0f;
		TitleCameraUtility::CalculateLookAtAngles(lobbySwing.cameraPosition, lobbySwing.lookAt, toYaw, toPitch);
		poseTo = { lobbySwing.cameraPosition, toYaw, toPitch };
		
		timers.time = 0.0f; // 遷移時間リセット
		timers.state = 0.0f; // タイマーリセット

		scene->SetState(State::TransitionToLobby); // ロビーへ遷移
		logoUI.exitLeft = logoUI.exitFade;          // Exitフェード開始
	}

	if (logoSprite) logoSprite->Update();

	// カメラ補間が終わると、UpdateTransitionToLobby で state_ が LobbyIdle に変わる
	if (state == State::TransitionToLobby)
	{
		// ここでステートクラス自体を Lobby にバトンタッチ
		scene->ChangeState(std::make_unique<TitleTransitionToLobby>());
	}
}

void TitleAttractState::Exit(TitleScene* scene)
{
	(void)scene;
}
