#pragma once

/// ===== 入力スナップショット（毎フレームPlayerが作る）=====
struct InputSnapshot
{
	// --- Move（-1..+1） ---
	float moveX = 0.0f;   // 右が＋
	float moveZ = 0.0f;   // 前が＋

	// --- Look（生データ） ---
	float lookMouseX = 0.0f; // マウスの相対移動（Input::GetMouseMoveX）
	float lookMouseY = 0.0f; // マウスの相対移動（Input::GetMouseMoveY）
	float lookPadX = 0.0f; // 右スティック（-1..+1）
	float lookPadY = 0.0f; // 右スティック（-1..+1）

	// --- Actions ---
	bool sprintHeld = false;
	bool jumpHeld = false;		// 押している間 true
	bool jumpPressed = false;	// 押した瞬間だけ true
	bool blinkPressed = false;

	bool aimHeld = false;
	bool aimPressed = false;

	bool fireHeld = false;
	bool firePressed = false;

	bool reloadPressed = false;
	bool meleePressed = false;

	// --- Utility ---
	int  weaponSwitch = 0; // -1 / 0 / +1（ホイールやDPADで武器切替）
	int  weaponSlotPressed = 0; // 1..6（数字キーでカテゴリ切替：1=Primary..6=Heavy）
	bool toggleFireModePressed = false; // 発射モード切替（例: Vキー）
	bool pausePressed = false;
};