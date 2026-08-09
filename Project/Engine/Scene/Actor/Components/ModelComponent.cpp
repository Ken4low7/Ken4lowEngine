#include "ModelComponent.h"
#include "SceneComponent.h"
#include "CameraComponent.h"
#include "Actor.h"
#include "AssetPathSelector.h"
#include "MaterialRepository.h"

#include <Camera.h>
#include <Matrix4x4.h>

#include <algorithm>
#include <exception>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	void ModelComponent::Initialize()
	{
		SceneComponent::Initialize();
		if (modelPath_.empty())
		{
			return;
		}

		try
		{
			object3D_ = std::make_unique<Object3D>();
			object3D_->Initialize(modelPath_);
		}
		catch (...)
		{
			object3D_.reset();
			return;
		}

		SyncTransformToObject3D();
		ApplyMaterialBinding();
	}

	void ModelComponent::Update(float deltaTime)
	{
		SceneComponent::Update(deltaTime);
		if (!object3D_)
		{
			return;
		}
		RefreshSharedMaterialBinding();
		SyncTransformToObject3D();
		object3D_->Update();
	}

	void ModelComponent::UpdateEditor(float deltaTime)
	{
		SceneComponent::UpdateEditor(deltaTime);
		if (!object3D_)
		{
			return;
		}
		RefreshSharedMaterialBinding();
		SyncTransformToObject3D();
		object3D_->Update(); // Gizmo変更後のWVPだけを更新し、物理やゲームロジックは進めない。
	}

	void ModelComponent::PostPhysicsUpdate([[maybe_unused]] float deltaTime)
	{
		if (!object3D_)
		{
			return;
		}
		RefreshSharedMaterialBinding();
		SyncTransformToObject3D();
		object3D_->Update();
	}

	void ModelComponent::Draw()
	{
		if (visible_ && object3D_)
		{
			object3D_->Draw();
		}
	}

	void ModelComponent::DrawShadow()
	{
		if (visible_ && IsCastShadowEnabled() && object3D_)
		{
			object3D_->DrawShadow();
		}
	}

	void ModelComponent::DrawImGui()
	{
		SceneComponent::DrawImGui();
#ifdef USE_IMGUI
		ImGui::SeparatorText("Model Component");
		ImGui::Text("現在のモデル: %s", modelPath_.empty() ? "未選択" : modelPath_.c_str());
		std::string selectedModelPath = modelPath_;
		if (AssetPathSelector::DrawAssetSelector("一覧から選択##ModelComponentModelPath", selectedModelPath, AssetType::Model))
		{
			SetModelPath(selectedModelPath);
		}
		ComponentPropertyUtility::DrawImGui(CreateProperties(false));
		DrawMaterialBindingImGui();
		ImGui::Text("Object3D: %s", object3D_ ? "Created" : "Not Created");
#endif // USE_IMGUI
	}

	void ModelComponent::Finalize()
	{
	}

	void ModelComponent::ToJson(nlohmann::json& outJson) const
	{
		SceneComponent::ToJson(outJson);
		outJson["Class"] = GetClassTypeName();
		ComponentPropertyUtility::ToJson(const_cast<ModelComponent*>(this)->CreateProperties(), outJson);
		if (materialBinding_.HasBinding())
		{
			outJson["Material"] = materialBinding_.ToJson();
		}
	}

	void ModelComponent::FromJson(const nlohmann::json& inJson)
	{
		SceneComponent::FromJson(inJson);
		ComponentPropertyUtility::FromJson(CreateProperties(), inJson);
		const auto materialIt = inJson.find("Material");
		if (materialIt != inJson.end() && materialIt->is_object())
		{
			materialBinding_.FromJson(*materialIt);
		}
		else
		{
			materialBinding_ = MaterialBinding{};
		}
		ApplyMaterialBinding();
	}

	void ModelComponent::SetModelPath(std::string_view modelPath)
	{
		const std::string newModelPath(modelPath);
		if (modelPath_ == newModelPath)
		{
			return;
		}

		modelPath_ = newModelPath;
		const bool hadObject = object3D_ != nullptr;
		object3D_.reset();
		if (!hadObject || modelPath_.empty())
		{
			return;
		}

		try
		{
			object3D_ = std::make_unique<Object3D>();
			object3D_->Initialize(modelPath_);
		}
		catch (...)
		{
			object3D_.reset();
			return;
		}

		object3D_->SetCamera(camera_);
		SyncTransformToObject3D();
		ApplyMaterialBinding();
	}

	void ModelComponent::SetCamera(Camera* camera)
	{
		camera_ = camera;
		if (object3D_)
		{
			object3D_->SetCamera(camera_);
		}
	}

	void ModelComponent::SetMaterialAssetId(std::string_view assetId)
	{
		materialBinding_.SetAssetId(assetId);
		ApplyMaterialBinding();
	}

	void ModelComponent::SetMaterialOverrideEnabled(bool enabled)
	{
		materialBinding_.SetUseOverride(enabled);
		ApplyMaterialBinding();
	}

	void ModelComponent::ApplyMaterialBinding()
	{
		if (!object3D_)
		{
			return;
		}
		materialRepositoryRevision_ = MaterialRepository::GetInstance()->GetRevision();
		if (!materialBinding_.HasBinding())
		{
			object3D_->ResetMaterialBinding();
			materialBindingStatus_ = "モデル既定Materialを使用中";
			return;
		}

		MaterialDesc resolvedDesc{};
		if (!materialBinding_.Resolve(resolvedDesc))
		{
			object3D_->ResetMaterialBinding();
			materialBindingStatus_ = "MaterialAssetが見つからないためモデル既定へフォールバック";
			return;
		}

		object3D_->ApplyMaterialDesc(resolvedDesc);
		materialBindingStatus_ = materialBinding_.IsUsingOverride()
			? "Component固有Material Overrideを使用中"
			: "共有MaterialAssetを使用中: " + materialBinding_.GetAssetId();
	}

	void ModelComponent::RefreshSharedMaterialBinding()
	{
		if (materialBinding_.GetAssetId().empty() || materialBinding_.IsUsingOverride())
		{
			return;
		}
		const uint64_t currentRevision = MaterialRepository::GetInstance()->GetRevision();
		if (currentRevision != materialRepositoryRevision_)
		{
			ApplyMaterialBinding();
		}
	}

	void ModelComponent::DrawMaterialBindingImGui()
	{
#ifdef USE_IMGUI
		if (Ken4lowEngine::DrawMaterialBindingImGui(materialBinding_, "ModelComponent"))
		{
			ApplyMaterialBinding();
		}
		ImGui::TextDisabled("状態: %s", materialBindingStatus_.c_str());
#endif // USE_IMGUI
	}

	void ModelComponent::SyncTransformToObject3D()
	{
		if (!object3D_)
		{
			return;
		}

		if (camera_ && dynamic_cast<const CameraComponent*>(GetParent()))
		{
			const Matrix4x4 cameraRotation = Matrix4x4::MakeRotateMatrix(camera_->GetRotate());
			const Vector3 cameraSpaceOffset = Vector3::Transform(GetLocalPosition(), cameraRotation);
			object3D_->SetTranslate(camera_->GetTranslate() + cameraSpaceOffset);
			object3D_->SetRotate(camera_->GetRotate() + GetLocalRotation());
			object3D_->SetScale(GetLocalScale());
			object3D_->SetFrustumCullingEnabled(false); // 一人称ViewModelはNear Plane付近でも通常ObjectのFrustum判定で消さない。
			return;
		}

		object3D_->SetFrustumCullingEnabled(true);
		object3D_->SetTranslate(GetWorldPosition());
		object3D_->SetRotate(GetWorldRotation());
		object3D_->SetScale(GetWorldScale());
	}

	std::vector<ComponentProperty> ModelComponent::CreateProperties(bool includeModelPath)
	{
		std::vector<ComponentProperty> properties = {
			{ "Visible", "表示", ComponentPropertyType::Bool, [this]() -> ComponentPropertyValue { return visible_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<bool>(&value)) { SetVisible(*typedValue); } } }
		};
		if (includeModelPath)
		{
			properties.insert(properties.begin(),
				{ "ModelPath", "モデルパス", ComponentPropertyType::String, [this]() -> ComponentPropertyValue { return modelPath_; }, [this](const ComponentPropertyValue& value) { if (const auto* typedValue = std::get_if<std::string>(&value)) { SetModelPath(*typedValue); } } });
		}
		return properties;
	}
} // namespace Ken4lowEngine
