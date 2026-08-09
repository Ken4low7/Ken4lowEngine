#include "CameraComponent.h"
#include "CameraManager.h"
#include <Camera.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void CameraComponent::Initialize()
	{
		SceneComponent::Initialize();
		if (!autoRegisterMainCamera_)
		{
			camera_ = nullptr;
			return;
		}

		ActivateAsMainCameraDriver();
	}

	void CameraComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		if (!CanDriveMainCamera()) return;
		SyncTransformToCamera();
		camera_->Update();
	}

	void CameraComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		if (!CanDriveMainCamera()) return;
		SyncTransformToCamera();
		camera_->Update(); // Edit中も位置だけは同期し、モデルのScaleをカメラ行列へ混ぜない。
	}

	void CameraComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!CanDriveMainCamera()) return;
		SyncTransformToCamera();
		camera_->Update(); // Physics補正後のRoot位置を最終Camera Transformへ反映する。
	}

	void CameraComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("Camera Component");
		bool autoRegister = autoRegisterMainCamera_;
		if (ImGui::Checkbox("Auto Register Main Camera", &autoRegister))
		{
			SetAutoRegisterMainCamera(autoRegister);
		}
		ImGui::Checkbox("Inherit Parent Rotation", &inheritParentRotation_);
		ImGui::Text("Main Camera Driver: %s", IsMainCameraDriver() ? "Yes" : "No");
		ImGui::TextDisabled("Camera scale is always fixed to 1,1,1.");
#endif // USE_IMGUI
	}

	void CameraComponent::Finalize()
	{
		if (mainCameraDriver_ == this)
		{
			mainCameraDriver_ = nullptr; // 破棄済みComponentをDriverとして残さない。
		}
		camera_ = nullptr;
	}

	void CameraComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		outJson["AutoRegisterMainCamera"] = autoRegisterMainCamera_;
		outJson["InheritParentRotation"] = inheritParentRotation_;
	}

	void CameraComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		if (inJson.contains("AutoRegisterMainCamera") && inJson["AutoRegisterMainCamera"].is_boolean())
		{
			SetAutoRegisterMainCamera(inJson["AutoRegisterMainCamera"].get<bool>());
		}
		if (inJson.contains("InheritParentRotation") && inJson["InheritParentRotation"].is_boolean())
		{
			inheritParentRotation_ = inJson["InheritParentRotation"].get<bool>();
		}
	}

	void CameraComponent::SetAutoRegisterMainCamera(bool autoRegister)
	{
		autoRegisterMainCamera_ = autoRegister;
		if (!autoRegisterMainCamera_)
		{
			if (mainCameraDriver_ == this) mainCameraDriver_ = nullptr;
			camera_ = nullptr;
			return;
		}

		ActivateAsMainCameraDriver();
	}

	void CameraComponent::ActivateAsMainCameraDriver()
	{
		if (!autoRegisterMainCamera_) return;

		camera_ = CameraManager::GetInstance()->GetMainCamera();
		if (!camera_) return;

		mainCameraDriver_ = this; // Main Cameraを書き換えるComponentを必ず1つへ限定する。
		SyncTransformToCamera();
		camera_->Update();
	}

	bool CameraComponent::CanDriveMainCamera() const
	{
		return autoRegisterMainCamera_ && camera_ && mainCameraDriver_ == this;
	}

	void CameraComponent::SyncTransformToCamera()
	{
		if (!camera_) return;
		camera_->SetTranslate(GetWorldPosition());
		camera_->SetRotate(inheritParentRotation_ ? GetWorldRotation() : GetLocalRotation());
		camera_->SetScale({ 1.0f, 1.0f, 1.0f }); // ActorやModelのScaleをCameraのView行列へ継承させない。
	}
} // namespace Ken4lowEngine
