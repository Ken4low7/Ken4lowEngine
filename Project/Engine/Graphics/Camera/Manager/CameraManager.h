#pragma once
#include "Engine/System/Audio/Listener/AudioListener.h"
#include "Matrix4x4.h"
#include "Vector3.h"

namespace Ken4lowEngine
{
	class Camera;
	class DebugCamera;

	class CameraManager
	{
	public:
		static CameraManager* GetInstance();
		void Initialize();
		void Finalize();
		void Update();
		void UpdateAudioListener(float deltaTime);

		void SetMainCamera(Camera* camera);
		void SetUseDebugCamera(bool useDebugCamera);

		Camera* GetMainCamera() const { return mainCamera_; }
		DebugCamera* GetDebugCamera() const { return debugCamera_; }
		bool IsUsingDebugCamera() const { return useDebugCamera_; }
		Matrix4x4 GetActiveViewMatrix() const;
		Matrix4x4 GetActiveProjectionMatrix() const;
		Matrix4x4 GetActiveViewProjectionMatrix() const;
		Vector3 GetActiveCameraPosition() const;
		Vector3 GetActiveCameraForward() const;
		float GetActiveNearClip() const;
		float GetActiveFarClip() const;
		float GetActiveFovY() const;
		float GetActiveAspectRatio() const;
		const AudioListener& GetAudioListener() const { return audioListener_; }
		Camera* GetRenderCamera() const { return mainCamera_; }

	private:
		CameraManager() = default;
		~CameraManager() = default;
		CameraManager(const CameraManager&) = delete;
		CameraManager& operator=(const CameraManager&) = delete;

		Camera* mainCamera_ = nullptr;
		DebugCamera* debugCamera_ = nullptr;
		bool useDebugCamera_ = false;
		bool editorCameraInitializedFromMain_ = false; // 最初のEdit表示だけGame Camera位置を引き継ぎ、その後はEditor Cameraを保持する。
		AudioListener audioListener_;
	};
} // namespace Ken4lowEngine
