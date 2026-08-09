#define NOMINMAX
#include "Camera.h"
#include <WinApp.h>
#include "GameViewportConstants.h"
#include <ParameterManager.h>
#include "Matrix4x4.h"
#include "Quaternion.h"
#include <numbers>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif // USE_IMGUI

namespace Ken4lowEngine
{

	/// -------------------------------------------------------------
	///						コンストラクタ
	/// -------------------------------------------------------------
	Camera::Camera() :
		fovY_(1.0f),
		aspectRatio_(GameViewportConstants::Aspect),
		nearClip_(0.1f), farClip_(1000.0f),
		worldMatrix_(Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_)),
		viewMatrix_(Matrix4x4::Inverse(worldMatrix_)),
		projectionMatrix_(Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_)),
		viewProjectionMatrix_(Matrix4x4::Multiply(viewMatrix_, projectionMatrix_))
	{
		worldTransform_.Initialize();
	}


	/// -------------------------------------------------------------
	///							更新処理
	/// -------------------------------------------------------------
	void Camera::Update()
	{
		if (target_ != worldTransform_.translate_)
		{
			viewMatrix_ = Matrix4x4::LookAt(
				worldTransform_.translate_,  // カメラの位置
				target_,                     // 注視点
				{ 0.0f, 1.0f, 0.0f });          // 上方向
		}
		else
		{
			// 通常の回転・平行移動
			worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
			viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);
		}

		// ビュー行列の計算処理
		worldMatrix_ = Matrix4x4::MakeAffineMatrix(worldTransform_.scale_, worldTransform_.rotate_, worldTransform_.translate_);
		viewMatrix_ = Matrix4x4::Inverse(worldMatrix_);

		// プロジェクション行列の更新
		projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
	}

	/// -------------------------------------------------------------
	///						Aspect比設定
	/// -------------------------------------------------------------
	void Camera::SetAspectRatio(const float aspectRatio)
	{
		if (aspectRatio <= 0.0f)
		{
			return;
		}

		// RenderTargetのAspect変更を次フレーム待ちにせずProjectionへ即時反映する。
		aspectRatio_ = aspectRatio;
		projectionMatrix_ = Matrix4x4::MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
		viewProjectionMatrix_ = Matrix4x4::Multiply(viewMatrix_, projectionMatrix_);
	}

	/// -------------------------------------------------------------
	///						ImGuiの描画
	/// -------------------------------------------------------------
	void Camera::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::Begin("Camera");

		ImGui::DragFloat3("Position", &worldTransform_.translate_.x, 0.01f);
		ImGui::DragFloat3("Rotation", &worldTransform_.rotate_.x, 0.005f);

		// 投影の基本パラメータ
		float fovDeg = fovY_ * 180.0f / std::numbers::pi_v<float>;
		if (ImGui::SliderFloat("FOV (deg)", &fovDeg, 40.0f, 100.0f, "%.1f")) {
			fovY_ = fovDeg * std::numbers::pi_v<float> / 180.0f;
		}
		if (ImGui::DragFloat("Near", &nearClip_, 0.001f, 0.01f, 0.2f, "%.3f")) {
			nearClip_ = std::max(0.001f, std::min(nearClip_, farClip_ - 0.01f));
		}
		if (ImGui::DragFloat("Far", &farClip_, 1.0f, 50.0f, 10000.0f)) {
			farClip_ = std::max(nearClip_ + 0.01f, farClip_);
		}
		if (ImGui::Button("Reset FOV/Clips")) {
			fovY_ = 60.0f * std::numbers::pi_v<float> / 180.0f;
			nearClip_ = 0.05f;
			farClip_ = 1000.0f;
		}
		ImGui::End();
#endif // USE_IMGUI
	}

	/// -------------------------------------------------------------
	///						前方ベクトルの取得
	/// -------------------------------------------------------------
	Vector3 Camera::GetForward() const
	{
		// 回転行列をオイラー角から生成（pitch: X, yaw: Y）
		Matrix4x4 rotMat = Matrix4x4::MakeRotateMatrix(worldTransform_.rotate_);

		// ローカルZ+方向を前方向として回転を適用
		Vector3 forward = Vector3::Transform({ 0.0f, 0.0f, 1.0f }, rotMat);
		return Vector3::Normalize(forward); // 念のため正規化
	}

	void Camera::SetForward(const Vector3& forward)
	{
		Vector3 f = forward;
		const float lenSq = f.x * f.x + f.y * f.y + f.z * f.z;
		if (lenSq <= 0.000001f)
		{
			return;
		}

		f = Vector3::Normalize(f);

		// yaw
		worldTransform_.rotate_.y = std::atan2(-f.x, f.z);

		// pitch
		const float xzLen = std::sqrt(f.x * f.x + f.z * f.z);
		worldTransform_.rotate_.x = std::atan2(-f.y, xzLen);

		// roll は維持しない
		worldTransform_.rotate_.z = 0.0f;

		// target も前方へ更新しておく
		target_ = worldTransform_.translate_ + f;
	}

} // namespace Ken4lowEngine
