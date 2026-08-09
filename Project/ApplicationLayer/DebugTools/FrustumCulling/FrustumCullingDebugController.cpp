#define NOMINMAX
#include "FrustumCullingDebugController.h"
#include "Camera.h"
#include "CameraManager.h"
#include "FrustumDebugRenderer.h"
#include "Object3DCommon.h"
#include "PostEffectManager.h"

#ifdef _DEBUG
#include "DebugCamera.h"
#endif

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
#include <numbers>

namespace
{
	constexpr float kDegToRad = std::numbers::pi_v<float> / 180.0f;
	constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;

	// MainCameraの初期位置。デバッグカメラでの操作を想定して、やや高い位置から見下ろす形
	K4E::Vector3 GetDefaultMainCameraPosition()
	{
		return { 0.0f, 3.0f, -12.0f };
	}

	// MainCameraの初期回転。デバッグカメラでの操作を想定して、やや見下ろす形
	K4E::Vector3 GetDefaultMainCameraRotation()
	{
		return { 8.0f * kDegToRad, 0.0f, 0.0f };
	}
}

/// -------------------------------------------------------------
///                         初期化処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::Initialize(bool resetMainCamera)
{
	// デバッグカメラの位置をMainCameraの初期位置にリセットする
	if (resetMainCamera)
	{
		// デバッグカメラの位置/回転をMainCameraの初期値にリセット
		ResetMainCamera();
	}
}

/// -------------------------------------------------------------
///                         更新処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::Update(float deltaTime)
{
	(void)deltaTime;
}

/// -------------------------------------------------------------
///                         描画処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::DrawDebug()
{
#ifdef _DEBUG

	// フラスタㇺワイヤーフレームがnullでないとき、表示フラグがONなら描画する
	if (!showFrustumWireframe_) return;

	// カリングに使用されているカメラのViewProjection行列を取得する
	const K4E::Matrix4x4 viewProjection = K4E::Object3DCommon::GetInstance()->GetCullingViewProjectionMatrix();

	// FrustumDebugRendererを使ってフラスタㇺワイヤーフレームを描画する
	K4E::FrustumDebugRenderer{}.Draw(viewProjection, frustumWireColor_);
#endif
}

/// -------------------------------------------------------------
///                         ImGui描画処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::DrawImGui()
{
#ifdef USE_IMGUI
	// 単体表示時もDocking可能な通常ウィンドウとして開く。
	if (ImGui::Begin("Frustum Culling Debug"))
	{
		DrawImGuiContent();
	}
	ImGui::End();
#endif
}

/// -------------------------------------------------------------
///                        ImGui描画処理（内容）
/// -------------------------------------------------------------
void FrustumCullingDebugController::DrawImGuiContent()
{
#ifdef USE_IMGUI
	K4E::Object3DCommon* object3DCommon = K4E::Object3DCommon::GetInstance();
	int cullingCameraIndex = static_cast<int>(object3DCommon->GetCullingCameraMode());
	const char* cullingCameraItems[] = { "MainCamera", "DebugCamera", "ActiveCamera" };

	bool frustumCullingEnabled = object3DCommon->IsFrustumCullingEnabled();
	bool boundsDebugVisible = object3DCommon->IsBoundsDebugVisible();
	float nearDistance = 0.0f;
	float farDistance = 0.0f;

	// カリングカメラのNear/Far距離を取得して表示する。
	GetCullingCameraClipDistances(nearDistance, farDistance);

	// カリング設定のUI
	if (ImGui::Checkbox("Frustum Culling 有効", &frustumCullingEnabled))
	{
		// フラスタムカリングのON/OFFを切り替えるときの処理
		object3DCommon->SetFrustumCullingEnabled(frustumCullingEnabled);
	}

	// カリングカメラの選択UI
	if (ImGui::Combo("カリングカメラ", &cullingCameraIndex, cullingCameraItems, IM_ARRAYSIZE(cullingCameraItems)))
	{
		// カリングカメラの選択が変更されたときの処理
		object3DCommon->SetCullingCameraMode(static_cast<K4E::Object3DCommon::CullingCameraMode>(cullingCameraIndex));
	}

	ImGui::Separator();
	ImGui::Text("ActiveCamera: %s", K4E::CameraManager::GetInstance()->IsUsingDebugCamera() ? "DebugCamera" : "MainCamera");
	ImGui::Text("MainCamera: %s", K4E::CameraManager::GetInstance()->GetMainCamera() ? "Available" : "None");
#ifdef _DEBUG
	ImGui::Text("DebugCamera: %s", K4E::CameraManager::GetInstance()->GetDebugCamera() ? "Available" : "None");
#else
	ImGui::Text("DebugCamera: Disabled in release builds");
#endif
	ImGui::Checkbox("Frustumワイヤー表示", &showFrustumWireframe_);
	if (ImGui::Checkbox("Bounds表示ON/OFF", &boundsDebugVisible))
	{
		object3DCommon->SetBoundsDebugVisible(boundsDebugVisible);
	}
	ImGui::ColorEdit4("Frustum Wire Color", &frustumWireColor_.x);
	ImGui::InputFloat("Near", &nearDistance, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);
	ImGui::InputFloat("Far", &farDistance, 0.0f, 0.0f, "%.3f", ImGuiInputTextFlags_ReadOnly);

	ImGui::Separator();
	ImGui::Text("Object単位カリング数: %d / %d", object3DCommon->GetCulledObjectCount(), object3DCommon->GetTotalObjectCount());
	ImGui::Text("StageObject単位カリング数: %d / %d", object3DCommon->GetCulledStageObjectCount(), object3DCommon->GetTotalStageObjectCount());
	ImGui::Text("Mesh単位カリング数: %d / %d", object3DCommon->GetCulledMeshCount(), object3DCommon->GetTotalMeshCount());
	ImGui::Text("DrawされたMesh数: %d", object3DCommon->GetDrawnMeshCount());
	ImGui::Text("カリングされたMesh数: %d", object3DCommon->GetCulledMeshCount());
	ImGui::Text("DrawされたObject数: %d", object3DCommon->GetDrawnObjectCount());
	ImGui::Text("DrawされたStageObject数: %d", object3DCommon->GetDrawnStageObjectCount());
	ImGui::Text("Bounds未設定でDrawしたObject数: %d", object3DCommon->GetMissingBoundsDrawnObjectCount());
	ImGui::TextWrapped("DebugCameraで外側から見て、カリングカメラをMainCameraにするとMainCameraのFrustum内にあるMesh / StageObjectだけ描画されます。");
	ImGui::TextWrapped("StageChunk Cullingを有効にすると、静的ステージMeshをXZグリッドChunk単位で先に判定してからMesh単位カリングを行います。");

	// MainCameraの設定UIを表示する処理
	DrawMainCameraImGui();
#endif
}

/// -------------------------------------------------------------
///                  ImGui描画処理（MainCamera設定）
/// -------------------------------------------------------------
void FrustumCullingDebugController::DrawMainCameraImGui()
{
#ifdef USE_IMGUI
	K4E::Camera* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera();
	if (!mainCamera)
	{
		ImGui::Separator();
		ImGui::Text("メインカメラが設定されていません。");
		return;
	}

	if (ImGui::CollapsingHeader("メインカメラの設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		K4E::Vector3 position = mainCamera->GetTranslate();
		K4E::Vector3 rotation = mainCamera->GetRotate();
		float fovDeg = mainCamera->GetFovY() * kRadToDeg;
		float nearClip = mainCamera->GetNearClip();
		float farClip = mainCamera->GetFarClip();
		float aspectRatio = mainCamera->GetAspectRatio();
		bool changed = false;

		changed |= ImGui::DragFloat3("座標", &position.x, 0.05f);
		changed |= ImGui::DragFloat3("回転", &rotation.x, 0.005f);
		changed |= ImGui::SliderFloat("FOV", &fovDeg, 10.0f, 140.0f, "%.1f deg");
		changed |= ImGui::DragFloat("Near Clip", &nearClip, 0.001f, 0.001f, 10.0f, "%.3f");
		changed |= ImGui::DragFloat("Far Clip", &farClip, 0.5f, 1.0f, 1000.0f, "%.1f");
		changed |= ImGui::DragFloat("アスペクト比", &aspectRatio, 0.001f, 0.1f, 4.0f, "%.3f");

		if (changed)
		{
			farClip = std::max(farClip, 0.011f);
			nearClip = std::clamp(nearClip, 0.001f, farClip - 0.01f);
			farClip = std::max(farClip, nearClip + 0.01f);
			aspectRatio = std::max(aspectRatio, 0.1f);
			mainCamera->SetTranslate(position);
			mainCamera->SetRotate(rotation);
			mainCamera->SetFovY(fovDeg * kDegToRad);
			mainCamera->SetNearClip(nearClip);
			mainCamera->SetFarClip(farClip);
			mainCamera->SetAspectRatio(aspectRatio);
			mainCamera->Update();
		}

		if (ImGui::Button("メインカメラをリセット"))
		{
			ResetMainCamera();
		}
#ifdef _DEBUG
		ImGui::SameLine();
		if (ImGui::Button("デバッグカメラをメインカメラにコピー"))
		{
			CopyDebugCameraToMainCamera();
		}
#endif
	}
#endif
}

/// -------------------------------------------------------------
///         DebugCameraの内容をMainCameraにコピーする処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::CopyDebugCameraToMainCamera()
{
#ifdef _DEBUG

	// MainCameraとDebugCameraを取得する
	K4E::Camera* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera();

	// DebugCameraはデバッグビルドでのみ存在するため、DebugCameraの取得処理はデバッグビルドでのみ有効にする。
	K4E::DebugCamera* debugCamera = K4E::CameraManager::GetInstance()->GetDebugCamera();

	// MainCameraとDebugCameraの両方が存在しない場合はコピーできないため、処理を中断する。
	if (!mainCamera || !debugCamera) return;

	mainCamera->SetTranslate(debugCamera->GetTranslate());	   // DebugCameraの位置をMainCameraにコピー
	mainCamera->SetRotate(debugCamera->GetRotate());		   // DebugCameraの回転をMainCameraにコピー
	mainCamera->SetFovY(debugCamera->GetFovY());			   // DebugCameraのFOVをMainCameraにコピー
	mainCamera->SetNearClip(debugCamera->GetNearClip());	   // DebugCameraのNearクリップ距離をMainCameraにコピー
	mainCamera->SetFarClip(debugCamera->GetFarClip());		   // DebugCameraのFarクリップ距離をMainCameraにコピー
	mainCamera->SetAspectRatio(GetCurrentWindowAspectRatio()); // Debug反映時も固定内部解像度の16:9を維持する。
	mainCamera->Update();									   // MainCameraの状態を更新して、ビュー行列や射影行列を再計算する
#endif
}

/// -------------------------------------------------------------
///         MainCameraを初期位置にリセットする処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::ResetMainCamera()
{
	// MainCameraを取得する
	K4E::Camera* mainCamera = K4E::CameraManager::GetInstance()->GetMainCamera();

	// MainCameraが存在しない場合はリセットできないため、処理を中断する。
	if (!mainCamera) return;

	mainCamera->SetTranslate(GetDefaultMainCameraPosition());  // MainCameraの位置を初期位置にリセット
	mainCamera->SetRotate(GetDefaultMainCameraRotation());	   // MainCameraの回転を初期回転にリセット
	mainCamera->SetFovY(60.0f * kDegToRad);					   // MainCameraのFOVをデフォルトの60度にリセット
	mainCamera->SetNearClip(0.1f);							   // MainCameraのNearクリップ距離をデフォルトの0.1にリセット
	mainCamera->SetFarClip(240.0f);							   // MainCameraのFarクリップ距離をデフォルトの240にリセット
	mainCamera->SetAspectRatio(GetCurrentWindowAspectRatio()); // MainCameraのアスペクト比を現在のウィンドウのアスペクト比にリセット
	mainCamera->Update();									   // MainCameraの状態を更新して、ビュー行列や射影行列を再計算する
}

/// -------------------------------------------------------------
///         カリングカメラのNear/Far距離を取得する処理
/// -------------------------------------------------------------
void FrustumCullingDebugController::GetCullingCameraClipDistances(float& nearDistance, float& farDistance) const
{
	// 初期値は安全なデフォルトに設定。通常はここからカメラの値で上書きされる。
	nearDistance = 0.0f;
	farDistance = 0.0f;

	// Object3DCommonで設定されたカリングカメラモードに応じて、MainCameraまたはDebugCameraのクリップ距離を取得して返す。
	const K4E::Object3DCommon::CullingCameraMode mode = K4E::Object3DCommon::GetInstance()->GetCullingCameraMode();
	auto* cameraManager = K4E::CameraManager::GetInstance();

	// ラムダ関数を使って、MainCameraとDebugCameraのクリップ距離を取得する処理を共通化。
	auto setMainCameraClips = [&]() -> bool
		{
			if (K4E::Camera* mainCamera = cameraManager->GetMainCamera())
			{
				nearDistance = mainCamera->GetNearClip();
				farDistance = mainCamera->GetFarClip();
				return true;
			}
			return false;
		};

	// DebugCameraはリリースビルドでは存在しないため、DebugCameraのクリップ距離を取得する処理はデバッグビルドでのみ有効にする。
	auto setDebugCameraClips = [&]() -> bool
		{
#ifdef _DEBUG
			// デバッグビルドの場合はDebugCameraのクリップ距離を取得して返す。取得に成功した場合は早期リターンする。
			if (K4E::DebugCamera* debugCamera = cameraManager->GetDebugCamera())
			{
				nearDistance = debugCamera->GetNearClip(); // DebugCameraのNearクリップ距離を取得してnearDistanceに設定
				farDistance = debugCamera->GetFarClip();   // DebugCameraのFarクリップ距離を取得してfarDistanceに設定
				return true;
			}
#endif
			// デバッグビルドでDebugCameraが取得できない場合や、リリースビルドの場合はここに到達するため、falseを返す。
			return false;
		};

	// カリングカメラモードに応じて、適切なカメラのクリップ距離を取得して返す。取得に成功した場合は早期リターンする。
	switch (mode)
	{
	case K4E::Object3DCommon::CullingCameraMode::MainCamera:
		// MainCameraモードの場合はMainCameraのクリップ距離を取得して返す。取得に成功した場合は早期リターンする。
		if (setMainCameraClips()) return;
		break;
	case K4E::Object3DCommon::CullingCameraMode::DebugCamera:
		// DebugCameraモードの場合はDebugCameraのクリップ距離を取得して返す。取得に成功した場合は早期リターンする。
		if (setDebugCameraClips()) return;
		break;
	case K4E::Object3DCommon::CullingCameraMode::ActiveCamera:
	default:
		if (cameraManager->IsUsingDebugCamera())
		{
			if (setDebugCameraClips()) return;
		}
		if (setMainCameraClips()) return;
		break;
	}

	setMainCameraClips();
}

/// --------------------------------------------------------------
///			現在のウィンドウのアスペクト比を取得する処理
/// --------------------------------------------------------------
float FrustumCullingDebugController::GetCurrentWindowAspectRatio() const
{
	// PostEffectManagerから現在のゲーム描画ターゲットの幅と高さを取得してアスペクト比を計算する。
	const auto* postEffectManager = K4E::PostEffectManager::GetInstance();

	// 高さが0の場合は1にして、極端な値になるのを防止する。
	const float height = static_cast<float>(std::max(postEffectManager->GetGameRenderTargetHeight(), 1u));

	// アスペクト比を計算して返す
	return static_cast<float>(postEffectManager->GetGameRenderTargetWidth()) / height;
}
