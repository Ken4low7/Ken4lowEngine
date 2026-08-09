#include "Input.h"
#include "WinApp.h"
#include <ResolutionManager.h>
#include <cassert>
#include <cstring> // memcpyを使うために追加

namespace Ken4lowEngine
{

#pragma comment(lib, "XInput.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

	XButtonIDs XButtons;

	/// -------------------------------------------------------------
	///				　XButtonIDsのコンストラクタ
	/// -------------------------------------------------------------
	XButtonIDs::XButtonIDs()
	{
		// アクションボタン
		A = 0;
		B = 1;
		X = 2;
		Y = 3;

		// DPADのボタン
		DPad_Up = 4;
		DPad_Down = 5;
		DPad_Left = 6;
		DPad_Right = 7;

		// Shoulderボタン
		L_Shoulder = 8;
		R_Shoulder = 9;

		// Thumbstick
		L_Thumbstick = 10;
		R_Thumbstick = 11;

		Start = 12; // 'START' ボタン
		Back = 13;  // 'BACK' ボタン

		// Triggerボタン
		L_Trigger = 14;
		R_Trigger = 15;
	}


	/// -------------------------------------------------------------
	///					　　シングルトンインスタンス
	/// -------------------------------------------------------------
	Input* Input::GetInstance()
	{
		static Input instance;
		return &instance;
	}


	/// -------------------------------------------------------------
	///					　　 	初期化処理
	/// -------------------------------------------------------------
	void Input::Initialize(WinApp* winApp)
	{
		// 借りてきたWinAppのインスタンスを記録
		winApp_ = winApp;

		HRESULT result{};

		// DirectInputのインスタンス生成
		result = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
		assert(SUCCEEDED(result));

		// キーボードデバイスの生成
		result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
		assert(SUCCEEDED(result));

		// 入力データ形式のセット
		result = keyboard->SetDataFormat(&c_dfDIKeyboard); // 標準形式
		assert(SUCCEEDED(result));

		// WindowsキーをOSへ渡すため、DISCL_NOWINKEYは付けずに非排他入力で取得する。
		result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		assert(SUCCEEDED(result));

		// キー配列の初期化
		memset(key, 0, sizeof(key));
		memset(keyPre, 0, sizeof(keyPre));

		// マウスデバイスの生成
		result = directInput->CreateDevice(GUID_SysMouse, mouseDevice_.GetAddressOf(), NULL);
		assert(SUCCEEDED(result));

		// マウスデバイスのフォーマット設定
		result = mouseDevice_->SetDataFormat(&c_dfDIMouse);
		assert(SUCCEEDED(result));

		// マウスデバイスの協調レベル設定
		result = mouseDevice_->SetCooperativeLevel(winApp_->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
		assert(SUCCEEDED(result));

		// マウスの座標を取得
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(winApp_->GetHwnd(), &point);
		mousePosition_.x = point.x;
		mousePosition_.y = point.y;

		// ゲームパッドの初期化
		for (int i = 0; i < GAMEPAD_BUTTON_NUM; i++)
		{
			prevButtonStates_[i] = false;
			buttonStates_[i] = false;
			buttonsTriger_[i] = false;
		}
	}


	/// -------------------------------------------------------------
	///					　　 	更新処理
	/// -------------------------------------------------------------
	void Input::Update()
	{
		HRESULT result{};

		// 前回のキー入力を保存
		memcpy(keyPre, key, sizeof(key));

		// キーボード情報の取得開始
		result = keyboard->Acquire();
		if (FAILED(result))
		{
			// デバイスの取得が失敗した場合はここで処理を終了する
			return;
		}

		// 全キーの入力情報を取得する
		result = keyboard->GetDeviceState(sizeof(key), key);
		if (FAILED(result))
		{
			// 取得が失敗した場合はキー配列をリセット
			memset(key, 0, sizeof(key));
		}

		prevMouseState_ = mouseState_; // マウスの状態を保存

		// マウスの情報の所得
		mouseDevice_->Acquire();
		result = mouseDevice_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseState_);
		if (FAILED(result))
		{
			// DirectInputが一時的に取れない場合はWin32の実ボタン状態でクリック欠落を防ぐ。
			memset(&mouseState_, 0, sizeof(mouseState_));
		}
		// ImGuiにMouseメッセージが捕捉されてもゲーム用クリック判定は物理ボタン状態を保持する。
		mouseState_.rgbButtons[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0 ? 0x80 : 0x00;
		mouseState_.rgbButtons[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ? 0x80 : 0x00;
		mouseState_.rgbButtons[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0 ? 0x80 : 0x00;

		// マウスの座標を取得
		UpdateMousePosition();

		// ゲームパッドの状態を取得
		state_ = GetGamePadState();

		// ゲームパッドのボタンの状態を更新
		for (int i = 0; i < 14; i++)
		{
			buttonStates_[i] = (state_.Gamepad.wButtons & XINPUT_Buttons[i]) == XINPUT_Buttons[i];
			buttonsTriger_[i] = !prevButtonStates_[i] && buttonStates_[i]; // トリガー判定
		}

		// トリガーの状態をボタンと同じように扱う
		buttonStates_[XButtons.L_Trigger] = state_.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		buttonStates_[XButtons.R_Trigger] = state_.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		buttonsTriger_[XButtons.L_Trigger] = !prevButtonStates_[XButtons.L_Trigger] && buttonStates_[XButtons.L_Trigger];
		buttonsTriger_[XButtons.R_Trigger] = !prevButtonStates_[XButtons.R_Trigger] && buttonStates_[XButtons.R_Trigger];

		// **ここで前回の状態を更新**
		memcpy(prevButtonStates_, buttonStates_, sizeof(prevButtonStates_));

		/// ----- マウスカーソルを画面中央に固定する処理 ----- ///
		if (lockCursor_)
		{
			// 最小化などで 0 になっている時は何もしない
			uint32_t w = winApp_->GetClientWidth();
			uint32_t h = winApp_->GetClientHeight();
			if (w > 0 && h > 0)
			{
				SetMousePosition((int)w / 2, (int)h / 2);
			}
		}
	}


	/// -------------------------------------------------------------
	///					　　キーの押下処理
	/// -------------------------------------------------------------
	bool Input::PushKey(BYTE keyNumber) const
	{
		if (!CanUseGameKeyboardInput(keyNumber))
		{
			// Editor操作中でもEscだけは既存Scene処理へ通して終了確認/Pauseを壊さない。
			return false;
		}

		return PushRawKey(keyNumber);
	}


	/// -------------------------------------------------------------
	///				　キーの押下処理（押した瞬間）
	/// -------------------------------------------------------------
	bool Input::TriggerKey(BYTE keyNumber) const
	{
		if (!CanUseGameKeyboardInput(keyNumber))
		{
			// Editor操作中でもEscだけは既存Scene処理へ通して終了確認/Pauseを壊さない。
			return false;
		}

		return TriggerRawKey(keyNumber);
	}

	bool Input::PushRawKey(BYTE keyNumber) const
	{
		// Editor専用ショートカットはゲーム入力抑制状態に影響されず取得する。
		return key[keyNumber] != 0;
	}

	bool Input::TriggerRawKey(BYTE keyNumber) const
	{
		// Editor専用ショートカットはゲーム入力抑制状態に影響されず取得する。
		return key[keyNumber] != 0 && keyPre[keyNumber] == 0;
	}


	/// -------------------------------------------------------------
	///				　 		マウスの座標を更新
	/// -------------------------------------------------------------
	void Input::UpdateMousePosition()
	{
		// マウスの座標を取得
		POINT point;
		GetCursorPos(&point);
		ScreenToClient(winApp_->GetHwnd(), &point);
		mousePosition_.x = point.x;
		mousePosition_.y = point.y;
	}


	/// -------------------------------------------------------------
	///				　 	マウスの押下状態を取得
	/// -------------------------------------------------------------
	bool Input::PushMouse(int button) const
	{
		if (!CanUseGameMouseInput())
		{
			// Editor側でゲーム入力が無効な間はマウス押下をゲームへ渡さない。
			return false;
		}

		if (mouseState_.rgbButtons[button])
		{
			return true;
		}

		return false;
	}


	/// -------------------------------------------------------------
	///				　 	マウスのトリガー状態を取得
	/// -------------------------------------------------------------
	bool Input::TriggerMouse(int button) const
	{
		if (!CanUseGameMouseInput())
		{
			// Editor側でゲーム入力が無効な間はマウストリガーをゲームへ渡さない。
			return false;
		}

		if (mouseState_.rgbButtons[button] && !prevMouseState_.rgbButtons[button])
		{
			return true;
		}

		return false;
	}


	/// -------------------------------------------------------------
	///				　 	マウスのリリース状態を取得
	/// -------------------------------------------------------------
	bool Input::ReleaseMouse(int button) const
	{
		if (!CanUseGameMouseInput())
		{
			// Editor側でゲーム入力が無効な間はマウスリリースをゲームへ渡さない。
			return false;
		}

		if (!mouseState_.rgbButtons[button] && prevMouseState_.rgbButtons[button])
		{
			return true;
		}

		return false;
	}


	/// -------------------------------------------------------------
	///				　 		マウスの座標を設定
	/// -------------------------------------------------------------
	void Input::SetMousePosition(int x, int y)
	{
		POINT point;
		point.x = x;
		point.y = y;
		ClientToScreen(winApp_->GetHwnd(), &point);
		SetCursorPos(point.x, point.y);
	}

	/// -------------------------------------------------------------
	///				　 		カーソルの表示・非表示
	/// -------------------------------------------------------------
	void Input::SetCursorVisible(bool visible)
	{
		if (cursorVisible_ == visible) { return; }
		cursorVisible_ = visible;

		if (visible)
		{
			// カウンタが 0以上になるまで TRUE を回す
			while (ShowCursor(TRUE) < 0) {}
		}
		else
		{
			// カウンタが 0未満になるまで FALSE を回す
			while (ShowCursor(FALSE) >= 0) {}
		}
	}

	/// -------------------------------------------------------------
	///				　 	マウスの座標を取得
	/// -------------------------------------------------------------
	Vector2 Input::GetMousePosition()
	{
		// 既存のゲーム側呼び出しもGetCursorPositionと同じ補正済み座標を返す
		return GetCursorPosition();
	}

	Vector2 Input::GetCursorPosition()
	{
#ifdef USE_IMGUI
		if (editorViewportMouseOverrideEnabled_)
		{
			// Editor座標と入力許可が有効な時だけGameViewportRenderTarget基準の座標を返す。
			return CanUseGameMouseInput() ? editorViewportMousePosition_ : Vector2(-1.0f, -1.0f);
		}
#endif // USE_IMGUI

		const Vector2 screenMousePos = { float(mousePosition_.x), float(mousePosition_.y) };
		// UI判定は内部1920x1080基準で行うため、現在ウィンドウ座標を内部座標へ戻して返す。
		return ResolutionManager::GetInstance()->ToLogical(screenMousePos);
	}

	void Input::SetEditorViewportMousePosition(const Vector2& position, bool valid)
	{
#ifdef USE_IMGUI
		// EditorWindowManagerで変換済みの座標と有効状態をInput側へ集約する。
		editorViewportMousePosition_ = position;
		editorViewportMousePositionValid_ = valid;
		editorViewportMouseOverrideEnabled_ = true;
#else
		// Release/GameではEditor由来のViewport座標を使わず通常のクライアント座標入力に固定する。
		(void)position;
		(void)valid;
		editorViewportMouseOverrideEnabled_ = false;
		editorViewportMousePositionValid_ = false;
#endif // USE_IMGUI
	}

	void Input::ClearEditorViewportMouseOverride()
	{
#ifdef USE_IMGUI
		// Game Preview ModeではMain Viewportを経由しないため、通常のクライアント座標入力へ戻す。
		editorViewportMouseOverrideEnabled_ = false;
		editorViewportMousePositionValid_ = false;
		editorViewportMousePosition_ = { 0.0f, 0.0f };
#endif // USE_IMGUI
	}

	void Input::SetGameInputEnabled(bool enabled)
	{
#ifdef USE_IMGUI
		// Editorのキャプチャ状態をInputへ渡し、ゲーム側マウス入力の入口を一元化する。
		gameInputEnabled_ = enabled;
#else
		// Release/GameではEditorInputModeに依存させず、ゲーム入力を常に許可する。
		(void)enabled;
		gameInputEnabled_ = true;
#endif // USE_IMGUI
	}

	int Input::GetMouseMoveX() const
	{
		// Main Viewport外やEditor操作中はゲーム側へマウス移動量を渡さない。
		return CanUseGameMouseInput() ? mouseState_.lX : 0;
	}

	int Input::GetMouseMoveY() const
	{
		// Main Viewport外やEditor操作中はゲーム側へマウス移動量を渡さない。
		return CanUseGameMouseInput() ? mouseState_.lY : 0;
	}

	int Input::GetMouseWheel() const
	{
		// Main Viewport外やEditor操作中はゲーム側へホイール入力を渡さない。
		return CanUseGameMouseInput() ? mouseState_.lZ : 0;
	}

	bool Input::CanUseGameKeyboardInput(BYTE keyNumber) const
	{
#ifdef USE_IMGUI
		// Escは既存Sceneの終了確認/戻る/Pause処理を守るためEditor抑制中でも通す。
		return keyNumber == DIK_ESCAPE || gameInputEnabled_;
#else
		// Release/GameではEditorInputModeを見ず、既存のキー入力を通常通り許可する。
		(void)keyNumber;
		return true;
#endif // USE_IMGUI
	}

	bool Input::CanUseGameMouseInput() const
	{
#ifdef USE_IMGUI
		// Editor経由ではキャプチャ許可とMain Viewport有効判定の両方を満たす時だけゲーム入力にする。
		return gameInputEnabled_ && (!editorViewportMouseOverrideEnabled_ || editorViewportMousePositionValid_);
#else
		// Release/GameではMain Viewport Hoveredを見ず、マウス入力を通常通り許可する。
		return true;
#endif // USE_IMGUI
	}

	bool Input::CanUseGamepadInput() const
	{
#ifdef USE_IMGUI
		// GameReleased中やMain Viewport外ではゲームパッド操作もゲームへ渡さない。
		return gameInputEnabled_;
#else
		// Release/GameではEditorInputModeを見ず、ゲームパッド入力を通常通り許可する。
		return true;
#endif // USE_IMGUI
	}

	void Input::SetLockCursor(bool lock)
	{
		if (lockCursor_ == lock) { return; }
		lockCursor_ = lock;
	}

	/// -------------------------------------------------------------
	///				　ゲームパッドの状態を取得
	/// -------------------------------------------------------------
	XINPUT_STATE Input::GetGamePadState()
	{
		XINPUT_STATE state;
		ZeroMemory(&state, sizeof(XINPUT_STATE));
		XInputGetState(0, &state); // ゲームパッドの状態を取得
		return state;
	}


	/// -------------------------------------------------------------
	///				　ゲームパッドの状態を更新
	/// -------------------------------------------------------------
	void Input::UpdateGamePadState()
	{
		memcpy(prevButtonStates_, buttonStates_, sizeof(prevButtonStates_));
	}


	/// -------------------------------------------------------------
	///				　ゲームパッドの接続状態を取得
	/// -------------------------------------------------------------
	bool Input::IsConnect()
	{
		ZeroMemory(&state_, sizeof(XINPUT_STATE));
		DWORD result = XInputGetState(0, &state_); // ゲームパッドの状態を取得
		return result == ERROR_SUCCESS;
	}


	/// -------------------------------------------------------------
	///				ゲームパッドのリリース状態を取得
	/// -------------------------------------------------------------
	bool Input::ReleaseButton(int button) const
	{
		if (!CanUseGamepadInput())
		{
			return false;
		}

		if (!buttonStates_[button] && prevButtonStates_[button])
		{
			return true;
		}

		return false;
	}


	/// -------------------------------------------------------------
	///				ゲームパッドの押下状態を取得
	/// -------------------------------------------------------------
	bool Input::PushButton(int button) const
	{
		if (!CanUseGamepadInput())
		{
			return false;
		}

		if (button == XButtons.L_Trigger)
		{
			return state_.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		}
		if (button == XButtons.R_Trigger)
		{
			return state_.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
		}
		return buttonStates_[button];
	}


	/// -------------------------------------------------------------
	///				ゲームパッドのトリガー状態を取得
	/// -------------------------------------------------------------
	bool Input::TriggerButton(int button) const
	{
		// GameReleased中やMain Viewport外ではゲームパッドトリガーをゲームへ渡さない。
		return CanUseGamepadInput() && buttonsTriger_[button];
	}


	/// -------------------------------------------------------------
	///					左スティックのデッドゾーン
	/// -------------------------------------------------------------
	bool Input::LStickInDeadZone() const
	{
		// 左スティックの値を取得
		short x = state_.Gamepad.sThumbLX;
		short y = state_.Gamepad.sThumbLY;

		// デッドゾーンの設定
		if (x > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || x < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		{
			return false;
		}

		if (y > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE || y < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		{
			return false;
		}

		return true;
	}


	/// -------------------------------------------------------------
	///					右スティックのデッドゾーン
	/// -------------------------------------------------------------
	bool Input::RStickInDeadZone() const
	{
		// 右スティックの値を取得
		short x = state_.Gamepad.sThumbRX;
		short y = state_.Gamepad.sThumbRY;

		// デッドゾーンの設定
		if (x > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || x < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
		{
			return false;
		}

		if (y > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE || y < -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
		{
			return false;
		}

		return true;
	}


	/// -------------------------------------------------------------
	///					左スティックの値を取得
	/// -------------------------------------------------------------
	Vector2 Input::GetLeftStick()
	{
		if (!CanUseGamepadInput())
		{
			return { 0.0f, 0.0f };
		}

		// 左スティックの値を取得
		short x = state_.Gamepad.sThumbLX;
		short y = state_.Gamepad.sThumbLY;

		return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
	}


	/// -------------------------------------------------------------
	///					右スティックの値を取得
	/// -------------------------------------------------------------
	Vector2 Input::GetRightStick()
	{
		if (!CanUseGamepadInput())
		{
			return { 0.0f, 0.0f };
		}

		// 右スティックの値を取得
		short x = state_.Gamepad.sThumbRX;
		short y = state_.Gamepad.sThumbRY;

		return Vector2(static_cast<float>(x) / 32768.0f, static_cast<float>(y) / 32768.0f);
	}


	/// -------------------------------------------------------------
	///					左トリガーの値を取得
	/// -------------------------------------------------------------
	float Input::GetLeftTrigger()
	{
		if (!CanUseGamepadInput())
		{
			return 0.0f;
		}

		// 左トリガーの値を取得
		BYTE trigger = state_.Gamepad.bLeftTrigger;

		if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		{
			return static_cast<float>(trigger) / 255.0f;
		}

		return 0.0f;
	}


	/// -------------------------------------------------------------
	///					右トリガーの値を取得
	/// -------------------------------------------------------------
	float Input::GetRightTrigger()
	{
		if (!CanUseGamepadInput())
		{
			return 0.0f;
		}

		// 左トリガーの値を取得
		BYTE trigger = state_.Gamepad.bRightTrigger;

		if (trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		{
			return static_cast<float>(trigger) / 255.0f;
		}

		return 0.0f;
	}


	/// -------------------------------------------------------------
	///						ゲームパッドの振動
	/// -------------------------------------------------------------
	void Input::SetVibration(float leftMotor, float rightMotor)
	{
		// モーターの振動設定
		XINPUT_VIBRATION vibration;
		ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

		vibration.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
		vibration.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);

		XInputSetState(0, &vibration);
	}


	/// -------------------------------------------------------------
	///				　ゲームパッドの振動を停止
	/// -------------------------------------------------------------
	void Input::StopVibration()
	{
		// モーターの振動の停止
		XINPUT_VIBRATION vibration;
		ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));

		vibration.wLeftMotorSpeed = 0;
		vibration.wRightMotorSpeed = 0;

		XInputSetState(0, &vibration);
	}

} // namespace Ken4lowEngine
