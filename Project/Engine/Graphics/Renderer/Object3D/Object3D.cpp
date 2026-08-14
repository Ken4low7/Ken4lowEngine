#define NOMINMAX
#include "Object3D.h"
#include "Object3DCommon.h"
#include "ImGuiManager.h"
#include "DirectXCommon.h"

#include <Model.h>
#include "ModelManager.h"

#include "CameraManager.h"
#include "AssimpLoader.h"
#include "ParameterManager.h"
#include "SkyBox.h"
#include "Wireframe.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace Ken4lowEngine
{
	namespace
	{
		bool ShouldUsePointSamplingForTexture(const std::string& texturePath)
		{
			std::string lowered = texturePath;
			std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return lowered.find("face") != std::string::npos ||
				lowered.find("pixel") != std::string::npos ||
				lowered.find("dot") != std::string::npos ||
				lowered.find("characters/") != std::string::npos ||
				lowered.find("skin") != std::string::npos; // Player・Enemyを含む低解像度キャラクタースキンは補間せず描画する。
		}
	}

	void Object3D::Initialize(const std::string& fileName)
	{
		dxCommon_ = DirectXCommon::GetInstance();
		camera_ = CameraManager::GetInstance()->GetMainCamera();
		SetModel(fileName);
		TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");
		environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");
		TextureManager::GetInstance()->LoadTexture("Effects/Masks/noise.dds");
		dissolveMaskHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("Effects/Masks/noise.dds");
		worldTransform_.Initialize();
		material_.Initialize();
		materialCullOverrideEnabled_ = false;
		materialTextureSlots_.Reset(); // 追加Texture Slotへ常に有効な中立SRVを設定する。
		InitializeCameraResource();
		InitializeDissolveResource();
		InitializeShadowResource();
		InitializeShadowParameterResource();
		AcquireShadowMapHandle();
	}

	void Object3D::Update()
	{
		material_.Update();
		worldTransform_.Update();
		cameraData_.worldPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const auto* lightMgr = LightManager::GetInstance();
		const Vector3 focusPos = cameraData_.worldPosition;
		const Matrix4x4 lightViewProjection = lightMgr->BuildShadowLightViewProjection(focusPos);
		UpdateShadowMatrix(lightViewProjection);
		shadowParameterData_.shadowBias = lightMgr->GetShadowBias();
		shadowParameterData_.normalBias = lightMgr->GetNormalBias();
		shadowParameterData_.shadowStrength = lightMgr->GetShadowStrength();
		shadowParameterData_.shadowMode = lightMgr->GetShadowReceiverMode();
		shadowParameterData_.shadowDebugMode = lightMgr->IsShadowMapDebugEnabled() ? 1u : (lightMgr->IsShadowFactorDebugEnabled() ? 2u : 0u);
	}

	void Object3D::UpdateWithWorldMatrix(const Matrix4x4& worldMatrix)
	{
		material_.Update();
		worldTransform_.UpdateWithWorldMatrix(worldMatrix);
		cameraData_.worldPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const auto* lightMgr = LightManager::GetInstance();
		const Vector3 focusPos = cameraData_.worldPosition;
		const Matrix4x4 lightViewProjection = lightMgr->BuildShadowLightViewProjection(focusPos);
		UpdateShadowMatrix(lightViewProjection);
		shadowParameterData_.shadowBias = lightMgr->GetShadowBias();
		shadowParameterData_.normalBias = lightMgr->GetNormalBias();
		shadowParameterData_.shadowStrength = lightMgr->GetShadowStrength();
		shadowParameterData_.shadowMode = lightMgr->GetShadowReceiverMode();
		shadowParameterData_.shadowDebugMode = lightMgr->IsShadowMapDebugEnabled() ? 1u : (lightMgr->IsShadowFactorDebugEnabled() ? 2u : 0u);
	}

	void Object3D::UpdateShadowMatrix(const Matrix4x4& lightViewProjection)
	{
		const Matrix4x4& worldMatrix = worldTransform_.matWorld_;
		shadowTransformData_.World = worldMatrix;
		shadowTransformData_.WVP = Matrix4x4::Multiply(worldMatrix, lightViewProjection);
		shadowTransformData_.WorldInversedTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrix));
		shadowParameterData_.lightViewProjection = lightViewProjection;
	}

	void Object3D::DrawImGui()
	{
#ifdef USE_IMGUI
		ImGui::PushID(this);
		if (ImGui::CollapsingHeader("Object", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::DragFloat3("Position##pos", &worldTransform_.translate_.x, 0.01f);
			ImGui::DragFloat3("Rotation##rot", &worldTransform_.rotate_.x, 0.01f);
			ImGui::DragFloat3("Scale##scl", &worldTransform_.scale_.x, 0.01f);
			ImGui::Checkbox("Object Frustum Culling", &frustumCullingEnabled_);
			const MaterialCullMode previousCullMode = material_.GetCullMode();
			material_.DrawImGui();
			if (material_.GetCullMode() != previousCullMode)
			{
				materialCullOverrideEnabled_ = true; // Editorで明示変更した後はImport値よりObject単位設定を優先する。
			}
		}
		ImGui::PopID();
#endif // USE_IMGUI
	}

	void Object3D::Draw()
	{
		DrawInternal(nullptr);
	}

	void Object3D::DrawMeshes(const std::vector<size_t>& meshIndices)
	{
		DrawInternal(&meshIndices);
	}

	void Object3D::DrawInternal(const std::vector<size_t>* meshIndices)
	{
		if (!model_) { return; }
		Object3DCommon* object3DCommon = Object3DCommon::GetInstance();

		if (!meshIndices)
		{
			const BoundingSphere objectBounds = GetWorldBounds();
			if (!object3DCommon->ShouldDrawObject(objectBounds, frustumCullingEnabled_, HasWorldBounds(), isStageObjectCullingUnit_))
			{
				DrawBoundsDebug(objectBounds, false);
				return;
			}
			DrawBoundsDebug(objectBounds, true);
		}

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		FrameUploadArena& frameUploadArena = dxCommon_->GetFrameUploadArena();
		const FrameUploadArena::Allocation cameraAllocation = frameUploadArena.AllocateConstant(cameraData_);
		const FrameUploadArena::Allocation dissolveAllocation = frameUploadArena.AllocateConstant(dissolveSetting_);
		const FrameUploadArena::Allocation shadowParameterAllocation = frameUploadArena.AllocateConstant(shadowParameterData_);
		if (!cameraAllocation.IsValid() || !dissolveAllocation.IsValid() || !shadowParameterAllocation.IsValid()) return;

		auto& meshes = model_->GetMeshes();
		std::array<std::vector<size_t>, 3> visibleMeshesByCullMode{};
		const size_t drawCount = meshIndices ? meshIndices->size() : meshes.size();
		for (size_t drawIndex = 0; drawIndex < drawCount; ++drawIndex)
		{
			const size_t meshIndex = meshIndices ? (*meshIndices)[drawIndex] : drawIndex;
			if (meshIndex >= meshes.size()) continue;
			const BoundingSphere meshBounds = GetMeshWorldBounds(meshIndex);
			const bool hasMeshBounds = HasMeshWorldBounds(meshIndex);
			const bool skipMeshFrustumForStageChunk = (meshIndices != nullptr) && isStageObjectCullingUnit_;
			const bool meshVisible = skipMeshFrustumForStageChunk ? true : object3DCommon->ShouldDrawMesh(meshBounds, frustumCullingEnabled_, hasMeshBounds);
			DrawBoundsDebug(meshBounds, meshVisible);
			if (!meshVisible) continue;

			const MaterialCullMode cullMode = ResolveSubmeshCullMode(meshIndex);
			visibleMeshesByCullMode[static_cast<size_t>(cullMode)].push_back(meshIndex);
		}

		auto bindSurfaceState = [&](MaterialCullMode cullMode)
		{
			if (alphaBlendEnabled_) object3DCommon->SetAlphaRenderSetting(cullMode);
			else object3DCommon->SetRenderSetting(cullMode);
			material_.SetPipeline();
			worldTransform_.SetPipeline();
			commandList->SetGraphicsRootConstantBufferView(3, cameraAllocation.gpuAddress);
			TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);
			commandList->SetGraphicsRootConstantBufferView(7, dissolveAllocation.gpuAddress);
			TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 8, dissolveMaskHandle_);
			commandList->SetGraphicsRootConstantBufferView(9, shadowParameterAllocation.gpuAddress);
			commandList->SetGraphicsRootDescriptorTable(10, shadowMapHandle_);
			materialTextureSlots_.BindAdditionalSlots(commandList, 12, 13, 14, 15);
		};

		for (size_t groupIndex = 0; groupIndex < visibleMeshesByCullMode.size(); ++groupIndex)
		{
			const auto& meshGroup = visibleMeshesByCullMode[groupIndex];
			if (meshGroup.empty()) continue;
			const MaterialCullMode cullMode = static_cast<MaterialCullMode>(groupIndex);
			bindSurfaceState(cullMode); // Cull ModeごとにRoot/PSOを一度だけ束縛し、SubMesh単位の両面情報を実描画へ反映する。

			for (const size_t meshIndex : meshGroup)
			{
				if (meshIndex < materialSRVs_.size())
				{
					TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[meshIndex]);
					material_.SetUsePointSampling(meshIndex < materialUsePointSampling_.size() ? materialUsePointSampling_[meshIndex] : false);
					material_.Update();
				}
				meshes[meshIndex].Draw();
			}
		}
	}

	void Object3D::DrawShadow()
	{
		if (!model_) { return; }
		UpdateShadowMatrix(LightManager::GetInstance()->GetActiveShadowPassLightViewProjection());
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const FrameUploadArena::Allocation shadowTransformAllocation = dxCommon_->GetFrameUploadArena().AllocateConstant(shadowTransformData_);
		if (!shadowTransformAllocation.IsValid()) return;

		auto& meshes = model_->GetMeshes();
		std::array<std::vector<size_t>, 3> meshesByCullMode{};
		for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
		{
			meshesByCullMode[static_cast<size_t>(ResolveSubmeshCullMode(meshIndex))].push_back(meshIndex);
		}

		for (size_t groupIndex = 0; groupIndex < meshesByCullMode.size(); ++groupIndex)
		{
			const auto& meshGroup = meshesByCullMode[groupIndex];
			if (meshGroup.empty()) continue;
			const MaterialCullMode cullMode = static_cast<MaterialCullMode>(groupIndex);
			Object3DCommon::GetInstance()->SetShadowMapRenderSetting(cullMode);
			commandList->SetGraphicsRootConstantBufferView(0, shadowTransformAllocation.gpuAddress); // ShadowもMainと同じSubMesh Surface契約で描画する。
			for (const size_t meshIndex : meshGroup) meshes[meshIndex].Draw();
		}
	}

	void Object3D::SetModel(const std::string& filePath)
	{
		model_ = ModelManager::GetInstance()->LoadModel(filePath);
		materialSRVs_.clear();
		if (model_)
		{
			materialSRVs_ = model_->GetMaterialSRVs();
			materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();
		}
	}

	void Object3D::ApplyMaterialDesc(const MaterialDesc& desc)
	{
		material_.ApplyDesc(desc);
		materialCullOverrideEnabled_ = true;
		materialTextureSlots_.ApplyDesc(desc);
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		if (model_)
		{
			materialSRVs_ = model_->GetMaterialSRVs();
			materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();
		}
		if (materialTextureSlots_.HasBaseColorOverride())
		{
			for (D3D12_GPU_DESCRIPTOR_HANDLE& baseColor : materialSRVs_) baseColor = materialTextureSlots_.ResolveBaseColor(baseColor);
		}
		materialUsePointSampling_.assign(materialSRVs_.size(), desc.legacy.usePointSampling);
	}

	void Object3D::ResetMaterialBinding()
	{
		material_.ResetToDefault();
		materialCullOverrideEnabled_ = false; // Reset後はモデルImport時のSubMesh Cull Modeへ戻す。
		materialTextureSlots_.Reset();
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		if (model_)
		{
			materialSRVs_ = model_->GetMaterialSRVs();
			materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();
		}
	}

	void Object3D::SetTextureForAll(const std::string& texturePath)
	{
		TextureManager::GetInstance()->LoadTexture(texturePath);
		auto h = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);
		for (auto& srv : materialSRVs_) srv = h;
		materialUsePointSampling_.assign(materialSRVs_.size(), ShouldUsePointSamplingForTexture(texturePath));
	}

	void Object3D::SetTextureForSubmesh(size_t index, const std::string& texturePath)
	{
		if (index >= materialSRVs_.size()) return;
		TextureManager::GetInstance()->LoadTexture(texturePath);
		materialSRVs_[index] = TextureManager::GetInstance()->GetSrvHandleGPU(texturePath);
		if (index >= materialUsePointSampling_.size()) materialUsePointSampling_.resize(materialSRVs_.size(), false);
		materialUsePointSampling_[index] = ShouldUsePointSamplingForTexture(texturePath);
	}

	size_t Object3D::GetSubmeshCount() const
	{
		return model_ ? model_->GetMeshes().size() : 0;
	}

	void Object3D::InitializeCameraResource()
	{
		cameraData_.worldPosition = camera_ ? camera_->GetTranslate() : CameraManager::GetInstance()->GetActiveCameraPosition();
	}

	void Object3D::InitializeDissolveResource()
	{
		dissolveSetting_.threshold = 1.0f;
		dissolveSetting_.edgeThickness = 0.0f;
		dissolveSetting_.edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	}

	void Object3D::InitializeShadowResource()
	{
		shadowTransformData_.World = Matrix4x4::MakeIdentity();
		shadowTransformData_.WVP = Matrix4x4::MakeIdentity();
		shadowTransformData_.WorldInversedTranspose = Matrix4x4::MakeIdentity();
	}

	void Object3D::InitializeShadowParameterResource()
	{
		shadowParameterData_.lightViewProjection = Matrix4x4::MakeIdentity();
		shadowParameterData_.shadowBias = 0.015f;
		shadowParameterData_.normalBias = 0.02f;
		shadowParameterData_.shadowStrength = 0.6f;
		shadowParameterData_.shadowMode = 0u;
		shadowParameterData_.shadowDebugMode = 0u;
	}

	BoundingSphere Object3D::GetWorldBounds() const
	{
		if (!HasWorldBounds()) return {};
		return TransformLocalBounds(model_->GetLocalBounds());
	}

	BoundingSphere Object3D::GetMeshWorldBounds(size_t meshIndex) const
	{
		if (!HasMeshWorldBounds(meshIndex)) return {};
		return TransformLocalBounds(model_->GetMeshLocalBounds(meshIndex));
	}

	BoundingSphere Object3D::TransformLocalBounds(const BoundingSphere& localBounds) const
	{
		BoundingSphere worldBounds{};
		worldBounds.center = Vector3::Transform(localBounds.center, worldTransform_.matWorld_);
		const float scaleX = std::sqrt(worldTransform_.matWorld_.m[0][0] * worldTransform_.matWorld_.m[0][0] + worldTransform_.matWorld_.m[0][1] * worldTransform_.matWorld_.m[0][1] + worldTransform_.matWorld_.m[0][2] * worldTransform_.matWorld_.m[0][2]);
		const float scaleY = std::sqrt(worldTransform_.matWorld_.m[1][0] * worldTransform_.matWorld_.m[1][0] + worldTransform_.matWorld_.m[1][1] * worldTransform_.matWorld_.m[1][1] + worldTransform_.matWorld_.m[1][2] * worldTransform_.matWorld_.m[1][2]);
		const float scaleZ = std::sqrt(worldTransform_.matWorld_.m[2][0] * worldTransform_.matWorld_.m[2][0] + worldTransform_.matWorld_.m[2][1] * worldTransform_.matWorld_.m[2][1] + worldTransform_.matWorld_.m[2][2] * worldTransform_.matWorld_.m[2][2]);
		worldBounds.radius = localBounds.radius * std::max({ scaleX, scaleY, scaleZ, 1.0f });
		return worldBounds;
	}

	bool Object3D::HasWorldBounds() const { return model_ && model_->HasLocalBounds(); }
	bool Object3D::HasMeshWorldBounds(size_t meshIndex) const { return model_ && model_->HasMeshLocalBounds(meshIndex); }

	MaterialCullMode Object3D::ResolveSubmeshCullMode(size_t meshIndex) const
	{
		MaterialCullMode cullMode = material_.GetCullMode();
		if (!materialCullOverrideEnabled_ && model_)
		{
			const auto& importedCullModes = model_->GetMaterialCullModes();
			if (meshIndex < importedCullModes.size()) cullMode = importedCullModes[meshIndex];
		}
		return ResolveMaterialCullModeForWorld(cullMode, worldTransform_.matWorld_); // Import値と負Scale補正を1箇所で解決する。
	}

	void Object3D::DrawBoundsDebug(const BoundingSphere& bounds, bool visible) const
	{
		if (!Object3DCommon::GetInstance()->IsBoundsDebugVisible() || bounds.radius <= 0.0f) return;
		const Vector4 color = visible ? Vector4{ 0.1f, 1.0f, 0.2f, 1.0f } : Vector4{ 1.0f, 0.15f, 0.1f, 1.0f };
		Wireframe::GetInstance()->DrawSphere(bounds.center, bounds.radius, color);
	}

	void Object3D::AcquireShadowMapHandle()
	{
		shadowMapHandle_ = dxCommon_->GetShadowMapSrvHandleGPU();
	}
} // namespace Ken4lowEngine
