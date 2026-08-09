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

	InputSnapshot BuildInputSnapshot(K4E::Input& input)
	{
		InputSnapshot snap{};

		// --------------------------------------------------
		// 移動入力
		// WASD を -1 ～ +1 の軸入力へ変換する
		// --------------------------------------------------
		if (input.PushKey(DIK_A)) { snap.moveX -= 1.0f; }
		if (input.PushKey(DIK_D)) { snap.moveX += 1.0f; }
		if (input.PushKey(DIK_S)) { snap.moveZ -= 1.0f; }
		if (input.PushKey(DIK_W)) { snap.moveZ += 1.0f; }

		// 斜め移動で速度が不自然に速くならないよう正規化する
		NormalizeClamp2(snap.moveX, snap.moveZ);

		// --------------------------------------------------
		// 視点入力
		// マウス移動量をそのまま視点操作用に保存する
		// --------------------------------------------------
		snap.lookMouseX = static_cast<float>(input.GetMouseMoveX());
		snap.lookMouseY = static_cast<float>(input.GetMouseMoveY());

		// --------------------------------------------------
		// アクション入力
		// Hold と Press を分けて管理する
		// --------------------------------------------------
		snap.sprintHeld = input.PushKey(DIK_LSHIFT);

		snap.jumpHeld = input.PushKey(DIK_SPACE);
		snap.jumpPressed = input.TriggerKey(DIK_SPACE);

		snap.blinkPressed = input.TriggerKey(DIK_LCONTROL);

		snap.aimHeld = input.PushMouse(1);      // 右クリック
		snap.aimPressed = input.TriggerMouse(1);

		snap.fireHeld = input.PushMouse(0);     // 左クリック
		snap.firePressed = input.TriggerMouse(0);

		snap.reloadPressed = input.TriggerKey(DIK_R);

		// --------------------------------------------------
		// 武器切替
		// 0  = 切替なし
		// +1 = 次の武器
		// -1 = 前の武器
		// --------------------------------------------------
		snap.weaponSwitch = 0;

		// WindowsのWheel値は±120単位でも返るため符号だけを使い、旧Playerと同じ操作方向へ変換する。
		const int wheelDelta = input.GetMouseWheel();

		if (wheelDelta > 0)
		{
			snap.weaponSwitch = -1;
		}
		else if (wheelDelta < 0)
		{
			snap.weaponSwitch = +1;
		}

		// --------------------------------------------------
		// 数字キーによる武器スロット指定
		// 0 は未入力
		// --------------------------------------------------
		snap.weaponSlotPressed = 0;

		if (input.TriggerKey(DIK_1)) { snap.weaponSlotPressed = 1; }
		else if (input.TriggerKey(DIK_2)) { snap.weaponSlotPressed = 2; }
		else if (input.TriggerKey(DIK_3)) { snap.weaponSlotPressed = 3; }
		else if (input.TriggerKey(DIK_4)) { snap.weaponSlotPressed = 4; }
		else if (input.TriggerKey(DIK_5)) { snap.weaponSlotPressed = 5; }
		else if (input.TriggerKey(DIK_6)) { snap.weaponSlotPressed = 6; }

		// --------------------------------------------------
		// その他入力
		// --------------------------------------------------
		snap.toggleFireModePressed = input.TriggerKey(DIK_V);
		snap.pausePressed = input.TriggerKey(DIK_ESCAPE);

		return snap;
	}
} // namespace Ken4lowEngine