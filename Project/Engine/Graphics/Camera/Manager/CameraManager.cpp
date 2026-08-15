#include "CameraManager.h"
#include "Camera.h"
#include "DebugCamera.h"
#include "Wireframe.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	CameraManager* CameraManager::GetInstance()
	{
		static CameraManager instance;
		return &instance;
	}

	void CameraManager::Initialize()
	{
		debugCamera_ = DebugCamera::GetInstance();
		useDebugCamera_ = false;
		editorCameraInitializedFromMain_ = false;
		renderViewOverrides_.clear();
	}

	void CameraManager::Finalize()
	{
		renderViewOverrides_.clear(); // Capture中断時も一時Viewを次Sceneへ持ち越さない。
		mainCamera_ = nullptr;
		debugCamera_ = nullptr;
		useDebugCamera_ = false;
		editorCameraInitializedFromMain_ = false;
	}

	void CameraManager::Update()
	{
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			debugCamera_->Update();
		}
#endif
	}

	void CameraManager::UpdateAudioListener(float deltaTime)
	{
		const Vector3 position = GetActiveCameraPosition();
		Vector3 forward{ 0.0f, 0.0f, 1.0f };

#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			forward = Vector3::Transform({ 0.0f, 0.0f, 1.0f }, Matrix4x4::MakeRotateMatrix(debugCamera_->GetRotate()));
		}
		else
#endif
			if (mainCamera_)
			{
				forward = mainCamera_->GetForward();
			}

		forward = Vector3::NormalizeSafe(forward, { 0.0f, 0.0f, 1.0f });
		Vector3 right = Vector3::NormalizeSafe(Vector3::Cross({ 0.0f, 1.0f, 0.0f }, forward), { 1.0f, 0.0f, 0.0f });
		Vector3 up = Vector3::NormalizeSafe(Vector3::Cross(forward, right), { 0.0f, 1.0f, 0.0f });
		audioListener_.UpdateFromTransform(position, forward, right, up, deltaTime);
	}

	void CameraManager::SetMainCamera(Camera* camera)
	{
		if (mainCamera_ != camera)
		{
			editorCameraInitializedFromMain_ = false;
		}
		mainCamera_ = camera;
	}

	void CameraManager::SetUseDebugCamera(bool useDebugCamera)
	{
#ifdef _DEBUG
		if (useDebugCamera && !editorCameraInitializedFromMain_ && debugCamera_ && mainCamera_)
		{
			debugCamera_->SetTranslate(mainCamera_->GetTranslate());
			debugCamera_->SetRotate(mainCamera_->GetRotate());
			debugCamera_->SetFovY(mainCamera_->GetFovY());
			debugCamera_->SetAspectRatio(mainCamera_->GetAspectRatio());
			debugCamera_->SetNearClip(mainCamera_->GetNearClip());
			debugCamera_->SetFarClip(mainCamera_->GetFarClip());
			debugCamera_->RefreshViewProjection();
			editorCameraInitializedFromMain_ = true; // 初回だけ現在のゲーム画面と同じ位置からEditor Camera操作を開始する。
		}
#endif
		useDebugCamera_ = useDebugCamera;
		Wireframe::GetInstance()->SetDebugCamera(useDebugCamera_); // モデルとWireframeを必ず同じActive Cameraで描画する。
	}

	void CameraManager::PushRenderViewOverride(const RenderViewOverride& renderView)
	{
		RenderViewOverride normalized = renderView;
		normalized.forward = Vector3::NormalizeSafe(renderView.forward, { 0.0f, 0.0f, 1.0f });
		normalized.nearClip = (std::max)(renderView.nearClip, 0.001f);
		normalized.farClip = (std::max)(renderView.farClip, normalized.nearClip + 0.001f);
		normalized.aspectRatio = (std::max)(renderView.aspectRatio, 0.001f); // Windowsのmaxマクロが有効でも標準関数として解決させる。
		renderViewOverrides_.push_back(normalized); // Probeの6面描画だけActive Viewを差し替え、Main/Debug Camera本体は変更しない。
	}

	void CameraManager::PopRenderViewOverride()
	{
		if (!renderViewOverrides_.empty())
		{
			renderViewOverrides_.pop_back();
		}
	}

	const CameraManager::RenderViewOverride* CameraManager::GetActiveRenderViewOverride() const
	{
		return renderViewOverrides_.empty() ? nullptr : &renderViewOverrides_.back();
	}

	Matrix4x4 CameraManager::GetActiveViewMatrix() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->view;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetViewMatrix();
#endif
		return mainCamera_ ? mainCamera_->GetViewMatrix() : Matrix4x4::MakeIdentity();
	}

	Matrix4x4 CameraManager::GetActiveProjectionMatrix() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->projection;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetProjectionMatrix();
#endif
		return mainCamera_ ? mainCamera_->GetProjectionMatrix() : Matrix4x4::MakeIdentity();
	}

	Matrix4x4 CameraManager::GetActiveViewProjectionMatrix() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->viewProjection;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetViewProjectionMatrix();
#endif
		return mainCamera_ ? mainCamera_->GetViewProjectionMatrix() : Matrix4x4::MakeIdentity();
	}

	Vector3 CameraManager::GetActiveCameraPosition() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->position;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetTranslate();
#endif
		return mainCamera_ ? mainCamera_->GetTranslate() : Vector3{ 0.0f, 0.0f, 0.0f };
	}

	Vector3 CameraManager::GetActiveCameraForward() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->forward;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_)
		{
			return Vector3::NormalizeSafe(Vector3::Transform({ 0.0f, 0.0f, 1.0f }, Matrix4x4::MakeRotateMatrix(debugCamera_->GetRotate())), { 0.0f, 0.0f, 1.0f });
		}
#endif
		return mainCamera_ ? mainCamera_->GetForward() : Vector3{ 0.0f, 0.0f, 1.0f };
	}

	float CameraManager::GetActiveNearClip() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->nearClip;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetNearClip();
#endif
		return mainCamera_ ? mainCamera_->GetNearClip() : 0.1f;
	}

	float CameraManager::GetActiveFarClip() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->farClip;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetFarClip();
#endif
		return mainCamera_ ? mainCamera_->GetFarClip() : 1000.0f;
	}

	float CameraManager::GetActiveFovY() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->fovY;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetFovY();
#endif
		return mainCamera_ ? mainCamera_->GetFovY() : 1.0f;
	}

	float CameraManager::GetActiveAspectRatio() const
	{
		if (const RenderViewOverride* renderView = GetActiveRenderViewOverride()) return renderView->aspectRatio;
#ifdef _DEBUG
		if (useDebugCamera_ && debugCamera_) return debugCamera_->GetAspectRatio();
#endif
		return mainCamera_ ? mainCamera_->GetAspectRatio() : 16.0f / 9.0f;
	}
} // namespace Ken4lowEngine
