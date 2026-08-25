#include "ModelComponent.h"
#include "SceneComponent.h"
#include "CameraComponent.h"
#include "PlanarReflectionComponent.h"
#include "Actor.h"
#include "AssetPathSelector.h"
#include "MaterialRepository.h"
#include "CameraManager.h"
#include "Engine/Graphics/Renderer/Environment/EnvironmentMapManager.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"
#include "Engine/Graphics/Renderer/Reflection/PlanarReflectionManager.h"
#include "Engine/Graphics/Renderer/Reflection/ReflectionProbeManager.h"

#include <Camera.h>
#include <Matrix4x4.h>

#include <exception>

#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	namespace
	{
		float CalculateForwardSortDepth(const Object3D& object3D)
		{
			const BoundingSphere bounds = object3D.GetWorldBoundsForCulling();
			const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
			const Vector3 cameraForward = CameraManager::GetInstance()->GetActiveCameraForward();
			const Vector3 toObject = {
				bounds.center.x - cameraPosition.x,
				bounds.center.y - cameraPosition.y,
				bounds.center.z - cameraPosition.z,
			};
			return toObject.x * cameraForward.x + toObject.y * cameraForward.y + toObject.z * cameraForward.z;
		}
	}

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

	bool ModelComponent::SubmitForwardBucket(ForwardRenderQueue& queue, MaterialBlendMode expectedBlendMode)
	{
		if (!visible_ || !IsActiveInHierarchy() || !object3D_)
		{
			return false;
		}

		if (object3D_->GetBlendMode() != expectedBlendMode)
		{
			return false;
		}

		ForwardRenderItem item = MakeForwardRenderItem(
			this,
			[](void* payload)
			{
				static_cast<ModelComponent*>(payload)->DrawWithReflectionBinding(); // Queue実行時に現在Viewと局所Reflectionを解決し、Capture後のMain Viewも復元する。
			},
			expectedBlendMode,
			CalculateForwardSortDepth(*object3D_));
		if (!queue.Submit(item))
		{
			return false;
		}

		lastForwardQueueSerial_ = queue.GetFrameSerial(); // Queueへ所有権を渡したModelは同じFrameのActor::Drawから除外する。
		return true;
	}

	bool ModelComponent::SubmitForwardOpaque(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Opaque);
	}

	bool ModelComponent::SubmitForwardMasked(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Masked);
	}

	bool ModelComponent::SubmitForwardTransparent(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Transparent);
	}

	bool ModelComponent::SubmitForwardAdditive(ForwardRenderQueue& queue)
	{
		return SubmitForwardBucket(queue, MaterialBlendMode::Additive);
	}

	void ModelComponent::Draw()
	{
		if (visible_ && object3D_)
		{
			ForwardRenderQueue* queue = ForwardRenderQueue::GetInstance();
			const bool alreadySubmittedToForwardQueue =
				queue->IsFrameActive() &&
				queue->GetFrameSerial() == lastForwardQueueSerial_;
			if (alreadySubmittedToForwardQueue)
			{
				return; // 透明系Bucketを後段実行できるよう、Queue所有中は直接Drawしない。
			}
			DrawWithReflectionBinding();
		}
	}

	void ModelComponent::DrawReflectionCapture()
	{
		if (!visible_ || !IsActiveInHierarchy() || !object3D_) return;
		DrawWithReflectionBinding(); // Capture Queue側のBlend順序に従い、透明・加算MaterialもReflection Cameraへ描画する。
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

	void ModelComponent::PrepareForCurrentRenderView()
	{
		if (!object3D_) return;
		RefreshWorldTransform();
		RefreshSharedMaterialBinding();
		SyncTransformToObject3D();
		object3D_->Update(); // Reflection Capture/Main ViewごとにWVPとCamera定数をDraw直前に作り直す。
	}

	void ModelComponent::DrawWithReflectionBinding()
	{
		if (!object3D_) return;
		PrepareForCurrentRenderView();

		EnvironmentMapManager* environmentManager = EnvironmentMapManager::GetInstance();
		const D3D12_GPU_DESCRIPTOR_HANDLE reflectionHandle =
			ReflectionProbeManager::GetInstance()->ResolveReflectionHandle(GetWorldPosition());
		EnvironmentMapManager::ScopedDrawOverride reflectionScope(environmentManager, reflectionHandle);

		PlanarReflectionManager* planarManager = PlanarReflectionManager::GetInstance();
		PlanarReflectionDrawSet planarDrawSet{};
		if (Actor* owner = GetOwner())
		{
			for (PlanarReflectionComponent* planar : owner->GetComponents<PlanarReflectionComponent>())
			{
				if (!planar || !planar->IsActiveInHierarchy() || !planar->IsEnabled()) continue;
				planarDrawSet.Add(planarManager->ResolveBinding(planar));
				if (planarDrawSet.count >= kMaxPlanarReflectionSurfacesPerDraw) break;
			}
		}
		PlanarReflectionManager::ScopedDrawBinding planarScope(planarManager, planarDrawSet); // 同Actorの最大6鏡面を1 Draw Packetへまとめ、Pixelごとに対応面を選択する。
		object3D_->Draw();
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