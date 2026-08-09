#pragma once
#include "WorldTransform.h"
#include "Quaternion.h"

namespace Ken4lowEngine
{
	/// -------------------------------------------------------------
	///						デバッグカメラクラス
	/// -------------------------------------------------------------
	class DebugCamera
	{
	public: /// ---------- メンバ関数 ---------- ///

		/// <summary>DebugCamera のシングルトンインスタンスを取得します。</summary>
		static DebugCamera* GetInstance();

		/// <summary>デバッグカメラを初期化します。</summary>
		void Initialize();

		/// <summary>終了処理を行います。</summary>
		void Finalize();

		/// <summary>カメラ入力とビュー射影行列を更新します。</summary>
		void Update();

		/// <summary>Editor Camera操作後の行列を即時更新します。</summary>
		void RefreshViewProjection();

		/// <summary>Editor Viewportから受け取ったローカル移動量と回転差分を反映します。</summary>
		void ApplyEditorNavigation(const Vector3& localMove, float pitchDelta, float yawDelta);

	private: /// ---------- メンバ関数 ---------- ///

		void Move();
		void UpdateViewProjection();

	public: /// ---------- 設定 ---------- ///

		void SetViewMatrix(const Matrix4x4& viewMatrix) { viewMatrix_ = viewMatrix; }
		void SetProjectionMatrix(const Matrix4x4& projectionMatrix) { projectionMatrix_ = projectionMatrix; }
		void SetViewProjectionMatrix(const Matrix4x4& viewProjectionMatrix) { viewProjectionMatrix_ = viewProjectionMatrix; }
		void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }
		void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }
		void SetFovY(float fovY) { fovY_ = fovY; }
		void SetAspectRatio(float aspectRatio);
		void SetNearClip(float nearClip) { nearClip_ = nearClip; }
		void SetFarClip(float farClip) { farClip_ = farClip; }

	public: /// ---------- 取得 ---------- ///

		Matrix4x4 GetViewMatrix() const { return viewMatrix_; }
		Matrix4x4 GetProjectionMatrix() const { return projectionMatrix_; }
		Matrix4x4 GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
		float GetNearClip() const { return nearClip_; }
		float GetFarClip() const { return farClip_; }
		Vector3 GetRotate() const { return worldTransform_.rotate_; }
		Vector3 GetTranslate() const { return worldTransform_.translate_; }
		float GetFovY() const { return fovY_; }
		float GetAspectRatio() const { return aspectRatio_; }

	private: /// ---------- メンバ変数 ---------- ///

		WorldTransform worldTransform_;
		Matrix4x4 worldMatrix_;
		Matrix4x4 rotateMatrix_;
		Matrix4x4 viewMatrix_;
		Matrix4x4 projectionMatrix_;
		float fovY_ = 0.0f;
		float aspectRatio_ = 0.0f;
		float nearClip_ = 0.0f;
		float farClip_ = 0.0f;
		Matrix4x4 viewProjectionMatrix_;
		Quaternion rotation_{};
		bool editorLookCaptureInitialized_ = false; // RMB開始時のカーソル再配置差分を1フレームだけ捨てる。

	private: /// ---------- コピー禁止 ---------- ///

		DebugCamera() = default;
		~DebugCamera() = default;
		DebugCamera(const DebugCamera&) = delete;
		DebugCamera& operator=(const DebugCamera&) = delete;
	};
} // namespace Ken4lowEngine
