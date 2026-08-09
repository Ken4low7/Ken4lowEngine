#pragma once
#include <Windows.h>
#include <wrl.h>
#include "Vector2.h"

#define DIRECTINPUT_VERSION		0x0800	// DirectInputのバージョン指定
#include <Xinput.h>						// ゲームコントローラーAPI
#include <dinput.h>

namespace Ken4lowEngine
{

	/// ---------- 前方宣言 ---------- ///
	class WinApp;

	// XINPUT_ButtonのGUID
	static const WORD XINPUT_Buttons[] =
	{
		XINPUT_GAMEPAD_A,			   // Aボタン
		XINPUT_GAMEPAD_B,			   // Bボタン
		XINPUT_GAMEPAD_X,			   // Xボタン
		XINPUT_GAMEPAD_Y,			   // Yボタン
		XINPUT_GAMEPAD_DPAD_UP,		   // 上ボタン
		XINPUT_GAMEPAD_DPAD_DOWN,	   // 下ボタン
		XINPUT_GAMEPAD_DPAD_LEFT,	   // 左ボタン
		XINPUT_GAMEPAD_DPAD_RIGHT,	   // 右ボタン
		XINPUT_GAMEPAD_LEFT_SHOULDER,  // LBボタン
		XINPUT_GAMEPAD_RIGHT_SHOULDER, // RBボタン
		XINPUT_GAMEPAD_LEFT_THUMB,	   // LTボタン
		XINPUT_GAMEPAD_RIGHT_THUMB,	   // RTボタン
		XINPUT_GAMEPAD_START,		   // スタートボタン
		XINPUT_GAMEPAD_BACK			   // バックボタン
	};

	// XButtonIDsの構造体
	struct XButtonIDs
	{
		// コンストラクタ
		XButtonIDs();

		// アクションボタン
		int A, B, X, Y;

		// Directional Pad(DPAD)ボタン
		int DPad_Up, DPad_Down, DPad_Left, DPad_Right;

		// Shoulderボタン
		int L_Shoulder, R_Shoulder;

		// Thumbstick ボタン
		int L_Thumbstick, R_Thumbstick;

		int Start; // 'START' ボタン
		int Back;  // 'BACK' ボタン

		// Triggerボタン
		int L_Trigger, R_Trigger;
	};

	/// -------------------------------------------------------------
	///					　		入力クラス
	/// -------------------------------------------------------------
	class Input
	{
	public: /// ---------- テンプレート ---------- ///

		// ComPtrのエイリアス
		template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	public: /// ---------- メンバ関数 ---------- ///

		// シングルトン
		static Input* GetInstance();

		// 初期化処理
		void Initialize(WinApp* winApp);

		// 更新処理
		void Update();

	public: /// ---------- キーボードのメンバ関数 ---------- ///

		/// <summary>
		/// キーの押下をチェック
		/// </summary>
		/// <param name="keyNumber">キー番号（ DIK_0 等 ）</param>
		/// <returns></returns>
		bool PushKey(BYTE keyNumber) const;

		/// <summary>
		/// キーのトリガーをチェック（押した瞬間）
		/// </summary>
		/// <param name="keyNumber">キー番号（ DIK_0 等 ）</param>
		/// <returns></returns>
		bool TriggerKey(BYTE keyNumber) const;

		/// <summary>
		/// Editorショートカット用にゲーム入力抑制を通さずキー押下を取得します。
		/// </summary>
		bool PushRawKey(BYTE keyNumber) const;
		bool TriggerRawKey(BYTE keyNumber) const;

	public: /// ---------- マウスのメンバ関数 ---------- ///

		/// <summary>
		/// マウスの座標を更新
		/// </summary>
		void UpdateMousePosition();

		/// <summary>
		/// マウスの押下状態を取得
		/// </summary>
		/// <param name="button"></param>
		/// <returns></returns>
		bool PushMouse(int button) const;

		/// <summary>
		/// マウスのトリガー状態を取得
		/// </summary>
		/// <param name="button"></param>
		/// <returns></returns>
		bool TriggerMouse(int button) const;

		/// <summary>
		/// マウスのリリース状態を取得
		/// </summary>
		/// <param name="button"></param>
		/// <returns></returns>
		bool ReleaseMouse(int button) const;

		/// <summary>
		/// マウスの座標を設定
		/// </summary>
		/// <param name="x"></param>
		/// <param name="y"></param>
		void SetMousePosition(int x, int y);

		/// <summary>
		/// 
		/// </summary>
		/// <param name="visible"></param>
		void SetCursorVisible(bool visible);

		/// <summary>
		/// マウスの座標を取得
		/// </summary>
		/// <returns></returns>
		Vector2 GetMousePosition();

		/// <summary>
		/// ゲーム基準のカーソル座標を取得します。
		/// </summary>
		Vector2 GetCursorPosition();

		/// <summary>
		/// エディタMain Viewport基準へ変換済みのマウス座標を設定します。
		/// </summary>
		void SetEditorViewportMousePosition(const Vector2& position, bool valid);

		/// <summary>
		/// Game Preview Modeなど、OSウィンドウ座標をそのままゲーム入力へ使う状態に戻します。
		/// </summary>
		void ClearEditorViewportMouseOverride();

		/// <summary>
		/// Editor側からゲーム入力を許可するか設定します。
		/// </summary>
		void SetGameInputEnabled(bool enabled);
		bool IsGameInputEnabled() const { return gameInputEnabled_; }

		// マウスのX座標を取得
		int GetMouseMoveX() const;

		// マウスのY座標を取得
		int GetMouseMoveY() const;

		// マウスのホイールの値を取得
		int GetMouseWheel() const;

		// ドラッグ状態を取得
		bool IsDragging(int button) const { return PushMouse(button); }

		// ドラッグの移動量を取得
		Vector2 GetDragDelta() const { return Vector2(static_cast<float>(mouseState_.lX), static_cast<float>(mouseState_.lY)); }

		// カーソルをロックするかどうかを設定
		void SetLockCursor(bool lock);
		bool GetLockCursor() const { return lockCursor_; }

		// カーソルをロックするかどうかを取得
		bool IsCursorLocked() const { return lockCursor_; }

		// カーソルの表示状態を取得
		bool IsCursorVisible() const { return cursorVisible_; }

	public: /// ---------- ゲームパッドのメンバ関数 ---------- ///

		/// <summary>
		/// ゲームパッドの状態を取得
		/// </summary>
		/// <returns></returns>
		XINPUT_STATE GetGamePadState();

		/// <summary>
		/// ゲームパッドの状態を更新
		/// </summary>
		void UpdateGamePadState();

		/// <summary>
		/// ゲームパッドの接続状態を取得
		/// </summary>
		/// <returns></returns>
		bool IsConnect();

		/// <summary>
		/// ゲームパッドのリリース状態を取得
		/// </summary>
		/// <param name="button"></param>
		/// <returns></returns>
		bool ReleaseButton(int button) const;

		/// <summary>
		/// ゲームパッドの押下状態を取得
		/// </summary>
		/// <param name="Button"></param>
		/// <returns></returns>
		bool PushButton(int button) const;

		/// <summary>
		/// ゲームパッドのトリガー状態を取得
		/// </summary>
		/// <param name="Button"></param>
		/// <returns></returns>
		bool TriggerButton(int button) const;

		/// <summary>
		/// 左スティックがデッドゾーン
		/// </summary>
		/// <returns></returns>
		bool LStickInDeadZone() const;

		/// <summary>
		/// 右スティックがデッドゾーン
		/// </summary>
		/// <returns></returns>
		bool RStickInDeadZone() const;

		/// <summary>
		/// 左スティックの値を取得
		/// </summary>
		/// <returns></returns>
		Vector2 GetLeftStick();

		/// <summary>
		/// 右スティックの値を取得
		/// </summary>
		/// <returns></returns>
		Vector2 GetRightStick();

		/// <summary>
		/// 左トリガーの値を取得
		/// </summary>
		/// <returns></returns>
		float GetLeftTrigger();

		/// <summary>
		/// 右トリガーの値を取得
		/// </summary>
		float GetRightTrigger();

		/// <summary>
		/// ゲームパッドの振動
		/// </summary>
		/// <param name="leftMotor"></param>
		/// <param name="rightMotor"></param>
		void SetVibration(float leftMotor, float rightMotor);

		/// <summary>
		/// ゲームパッドの振動を停止
		/// </summary>
		void StopVibration();

	private: /// ---------- メンバ変数 ---------- ///

		bool CanUseGameKeyboardInput(BYTE keyNumber) const;
		bool CanUseGameMouseInput() const;
		bool CanUseGamepadInput() const;

		// WindowsAPI
		WinApp* winApp_ = nullptr;

	private: /// ---------- キーボードのメンバ変数 ---------- ///

		ComPtr<IDirectInput8> directInput;	  // DirectInputのインスタンス
		ComPtr<IDirectInputDevice8> keyboard; // キーボードのデバイス
		BYTE key[256] = {};					  // キーボードの入力状態
		BYTE keyPre[256] = {};				  // 前フレームのキーボードの入力状態

	private: /// ---------- マウスのメンバ変数 ---------- ///

		ComPtr<IDirectInputDevice8> mouseDevice_; // マウスデバイス
		DIMOUSESTATE mouseState_;				  // マウスの状態
		DIMOUSESTATE prevMouseState_;			  // 前フレームのマウスの状態
		POINT mousePosition_ = {};				  // マウスの座標
		Vector2 editorViewportMousePosition_ = { 0.0f, 0.0f }; // Main Viewport基準へ変換済みのマウス座標
		bool editorViewportMousePositionValid_ = false;	  // Main Viewport内かつImGui未操作中だけtrueにする
		bool editorViewportMouseOverrideEnabled_ = false; // エディタ経由のマウス座標をゲーム入力へ反映するかどうか
		bool gameInputEnabled_ = true; // Editorからゲーム入力を一括で許可/抑制するためのフラグ
		bool lockCursor_ = false;				  // マウスカーソルをロックするかどうか
		bool cursorVisible_ = true;				  // マウスカーソルの表示状態

	private: /// ---------- ゲームパッドのメンバ変数 ---------- ///

		XINPUT_STATE state_;						// ゲームパッドの状態
		static const int GAMEPAD_BUTTON_NUM = 16;	// ゲームパッドのボタンの数
		bool buttonStates_[GAMEPAD_BUTTON_NUM];		// ゲームパッドのボタンの状態
		bool prevButtonStates_[GAMEPAD_BUTTON_NUM]; // 前フレームのゲームパッドのボタンの状態
		bool buttonsTriger_[GAMEPAD_BUTTON_NUM];	// ゲームパッドのトリガーの状態

	private: /// ---------- コピー禁止 ---------- ///

		Input() = default;
		~Input() = default;
		Input(const Input&) = delete;
		const Input& operator=(const Input&) = delete;
	};

	extern XButtonIDs XButtons;
} // namespace Ken4lowEngine
