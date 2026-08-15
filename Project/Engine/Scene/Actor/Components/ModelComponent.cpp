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

#include <algorithm>
#include <cmath>
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

		Vector3 ReflectVector(const Vector3& value, const Vector3& normal)
		{
			return value - normal * (2.0f * Vector3::Dot(value, normal));
		}

		Vector3 ReflectPoint(const Vector3& point, const Vector3& planePoint, const Vector3& planeNormal)
		{
			const float signedDistance = Vector3::Dot(point - planePoint, planeNormal);
			return point - planeNormal * (2.0f * signedDistance);
		}

		Matrix4x4 BuildPlanarReflectionViewProjection(const PlanarReflectionComponent& planar)
		{
			CameraManager* cameraManager = CameraManager::GetInstance();
			const Vector3 planeNormal = Vector3::NormalizeSafe(planar.GetPlaneNormal(), { 0.0f, 1.0f, 0.0f });
			const Vector3 cameraPosition = cameraManager->GetActiveCameraPosition();
			const Vector3 cameraForward = Vector3::NormalizeSafe(
				cameraManager->GetActiveCameraForward(),
				{ 0.0f, 0.0f, 1.0f });

			Vector3 referenceUp{ 0.0f, 1.0f, 0.0f };
			if (std::fabs(Vector3::Dot(referenceUp, cameraForward)) > 0.98f)
			{
				referenceUp = { 0.0f, 0.0f, 1.0f };
			}
			const Vector3 cameraRight = Vector3::NormalizeSafe(
				Vector3::Cross(referenceUp, cameraForward),
				{ 1.0f, 0.0f, 0.0f });
			const Vector3 cameraUp = Vector3::NormalizeSafe(
				Vector3::Cross(cameraForward, cameraRight),
				{ 0.0f, 1.0f, 0.0f });

			const Vector3 reflectedPosition = ReflectPoint(cameraPosition, planar.GetWorldPosition(), planeNormal);
			const Vector3 reflectedForward = Vector3::NormalizeSafe(
				ReflectVector(cameraForward, planeNormal),
				{ 0.0f, 0.0f, 1.0f });
			const Vector3 reflectedUp = Vector3::NormalizeSafe(
				ReflectVector(cameraUp, planeNormal),
				{ 0.0f, 1.0f, 0.0f });

			const Matrix4x4 reflectedView = Matrix4x4::LookAt(
				reflectedPosition,
				reflectedPosition + reflectedForward,
				reflectedUp);
			return Matrix4x4::Multiply(reflectedView, cameraManager->GetActiveProjectionMatrix());
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
		const MaterialBlendMode blendMode = object3D_->GetBlendMode();
		if (blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive)
		{
			return; // 初期Reflection CaptureはDepthが安定するOpaque/Maskedだけを対象にし、透明物は後続拡張へ分離する。
		}
		DrawWithReflectionBinding();
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
		PlanarReflectionBinding planarBinding{};
		if (Actor* owner = GetOwner())
		{
			if (PlanarReflectionComponent* planar = owner->GetComponent<PlanarReflectionComponent>();
				planar && planar->IsActiveInHierarchy() && planar->IsEnabled())
			{
				planarBinding = planarManager->ResolveBinding(planar);
				if (planarBinding.valid)
				{
					planarBinding.planeNormal = planar->GetPlaneNormal();
					planarBinding.reflectedViewProjection = BuildPlanarReflectionViewProjection(*planar); // 鏡面PixelをWorld位置から反射Cameraへ再投影して正しい鏡像UVを作る。
				}
			}
		}
		PlanarReflectionManager::ScopedDrawBinding planarScope(planarManager, planarBinding); // 同じActorの鏡面だけReflection TextureをDraw区間へ限定して公開する。
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
