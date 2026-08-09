#define NOMINMAX
#include "DebugCamera.h"
#include "WinApp.h"
#include "GameViewportConstants.h"
#include "GameTimer.h"
#include "Input.h"
#include "ParameterManager.h"

#ifdef USE_IMGUI
#include "../../../Editor/EditorWindowManager.h"
#include <imgui.h>
#endif

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	namespace
	{
#ifdef USE_IMGUI
		void CaptureCursorAtMainViewportCenter()
		{
			const EditorViewportRect& viewportRect = EditorWindowManager::GetInstance()->GetMainViewportRect();
			if (!viewportRect.valid) return;

			const ImVec2 mainViewportPosition = ImGui::GetMainViewport()->Pos;
			POINT clientCenter{
				static_cast<LONG>(std::lround((viewportRect.screenMin.x + viewportRect.screenMax.x) * 0.5f - mainViewportPosition.x)),
				static_cast<LONG>(std::lround((viewportRect.screenMin.y + viewportRect.screenMax.y) * 0.5f - mainViewportPosition.y)),
			};
			ClientToScreen(WinApp::GetInstance()->GetHwnd(), &clientCenter);
			SetCursorPos(clientCenter.x, clientCenter.y);
			ImGui::SetMouseCursor(ImGuiMouseCursor_None);
			SetCursor(nullptr); // RMB中はMain Viewport中央へ固定し、ImGuiとWin32の両方でカーソルを隠す。
		}
#endif
	}

	DebugCamera* DebugCamera::GetInstance()
	{
		static DebugCamera instance;
		return &instance;
	}

	void DebugCamera::Initialize()
	{
		worldTransform_.Initialize();
		worldTransform_.translate_ = { 0.0f, 0.0f, -20.0f };
		fovY_ = 1.0f;
		aspectRatio_ = GameViewportConstants::Aspect;
		nearClip_ = 0.1f;
		farClip_ = 1000.0f;
		editorLookCaptureInitialized_ = false;
		UpdateViewProjection();
	}

	void DebugCamera::Finalize()
	{
		worldTransform_ = {};
		worldMatrix_ = {};
		rotateMatrix_ = {};
		viewMatrix_ = {};
		projectionMatrix_ = {};
		viewProjectionMatrix_ = {};
		rotation_ = {};
		fovY_ = 0.0f;
		aspectRatio_ = 0.0f;
		nearClip_ = 0.0f;
		farClip_ = 0.0f;
		editorLookCaptureInitialized_ = false;
	}

	void DebugCamera::SetAspectRatio(float aspectRatio)
	{
		if (aspectRatio <= 0.0f)
		{
			return;
		}
		aspectRatio_ = aspectRatio;
		UpdateViewProjection();
	}

	void DebugCamera::Update()
	{
		const bool rightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		if (!rightMouseDown)
		{
			editorLookCaptureInitialized_ = false; // Cursor Lockではなく物理RMBの解放をEditor Look終了条件にする。
		}
		Move();
		UpdateViewProjection();
	}

	void DebugCamera::RefreshViewProjection()
	{
		UpdateViewProjection();
	}

	void DebugCamera::ApplyEditorNavigation(const Vector3& localMove, float pitchDelta, float yawDelta)
	{
		Input* input = Input::GetInstance();
		const bool rightMouseDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		Vector3 editorMove = localMove;

		if (input && rightMouseDown)
		{
			if (!editorLookCaptureInitialized_)
			{
				pitchDelta = 0.0f;
				yawDelta = 0.0f;
				editorLookCaptureInitialized_ = true; // RMBを押した瞬間の不連続な差分だけを1フレーム捨てる。
			}
			else
			{
				const Vector2 rawDelta = input->GetDragDelta();
				pitchDelta = std::clamp(rawDelta.y * 0.003f, -0.15f, 0.15f);
				yawDelta = std::clamp(-rawDelta.x * 0.003f, -0.15f, 0.15f); // Cursor再配置によるImGui差分を使わずDirectInput相対移動だけで回転する。
			}

			editorMove.y = 0.0f; // 旧Q/Eの上下移動を無効化し、Space/Ctrlへ操作を統一する。
			const float deltaTime = std::clamp(GameTimer::GetInstance()->GetDeltaTime(), 1.0f / 240.0f, 1.0f / 15.0f);
			const float moveSpeed = input->PushRawKey(DIK_LSHIFT) ? 24.0f : 8.0f;
			const float verticalStep = moveSpeed * deltaTime;
			if (input->PushRawKey(DIK_SPACE)) editorMove.y += verticalStep;
			if (input->PushRawKey(DIK_LCONTROL) || input->PushRawKey(DIK_RCONTROL)) editorMove.y -= verticalStep;

			input->SetLockCursor(false); // Input標準のウィンドウ中央固定を無効化し、Editor側でMain Viewport中央へ固定する。
#ifdef USE_IMGUI
			CaptureCursorAtMainViewportCenter();
#endif
		}

		worldTransform_.rotate_.x = std::clamp(worldTransform_.rotate_.x + pitchDelta, -1.5533f, 1.5533f);
		worldTransform_.rotate_.y += yawDelta;

		const Matrix4x4 cameraRotation = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);
		worldTransform_.translate_ += Vector3::Transform(editorMove, cameraRotation);
		UpdateViewProjection();
	}

	void DebugCamera::Move()
	{
		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (Input::GetInstance()->PushKey(DIK_W)) { move.z += 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_S)) { move.z -= 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_A)) { move.x -= 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_D)) { move.x += 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_SPACE)) { move.y += 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_LCONTROL) || Input::GetInstance()->PushKey(DIK_RCONTROL)) { move.y -= 0.2f; }
		if (Input::GetInstance()->PushKey(DIK_UP)) { worldTransform_.rotate_.x -= 0.02f; }
		if (Input::GetInstance()->PushKey(DIK_DOWN)) { worldTransform_.rotate_.x += 0.02f; }
		if (Input::GetInstance()->PushKey(DIK_LEFT)) { worldTransform_.rotate_.y += 0.02f; }
		if (Input::GetInstance()->PushKey(DIK_RIGHT)) { worldTransform_.rotate_.y -= 0.02f; }
		move = Vector3::Transform(move, rotateMatrix_);
		worldTransform_.translate_ += move;
	}

	void DebugCamera::UpdateViewProjection()
	{
		rotateMatrix_ = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);
		worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
		viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);
		projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
	}
} // namespace Ken4lowEngine
