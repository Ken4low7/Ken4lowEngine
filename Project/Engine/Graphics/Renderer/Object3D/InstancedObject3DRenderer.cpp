#define NOMINMAX
#include "InstancedObject3DRenderer.h"
#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "SRVManager.h"
#include "TextureManager.h"
#include "Model.h"
#include "ModelManager.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "Frustum.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Ken4lowEngine
{
	InstancedObject3DRenderer::~InstancedObject3DRenderer()
	{
		Finalize();
	}

	void InstancedObject3DRenderer::Initialize(const std::string& modelPath, size_t maxInstanceCount)
	{
		if (maxInstanceCount == 0 || maxInstanceCount > UINT32_MAX)
		{
			throw std::invalid_argument("InstancedObject3DRenderer maxInstanceCount is invalid");
		}

		Finalize();
		dxCommon_ = DirectXCommon::GetInstance();
		model_ = ModelManager::GetInstance()->LoadModel(modelPath);
		if (!model_) { throw std::runtime_error("InstancedObject3DRenderer failed to load model"); }

		maxInstanceCount_ = maxInstanceCount;
		instanceResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(InstanceData) * maxInstanceCount_);
		instanceResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedInstances_));
		instanceResource_->SetName(L"InstancedObject3DRenderer.InstanceBuffer");

		auto* srvManager = SRVManager::GetInstance();
		instanceSrvIndex_ = srvManager->Allocate();
		srvManager->CreateSRVForStructureBuffer(instanceSrvIndex_, instanceResource_.Get(), static_cast<UINT>(maxInstanceCount_), sizeof(InstanceData));

		perViewResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(PerViewData));
		perViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&perViewData_));
		cameraResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU));
		cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));
		dissolveResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(DissolveSetting));
		dissolveResource_->Map(0, nullptr, reinterpret_cast<void**>(&dissolveData_));
		shadowParameterResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ShadowParameterForGPU));
		shadowParameterResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowParameterData_));

		material_.Initialize();
		materialTextureSlots_.Reset();
		RestoreModelMaterials(); // Binding未指定時は共有Modelが読み込んだMaterial Textureを使用する。

		TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");
		environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");
		TextureManager::GetInstance()->LoadTexture("Effects/Masks/noise.dds");
		dissolveMaskHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("Effects/Masks/noise.dds");

		dissolveData_->threshold = 1.0f;
		dissolveData_->edgeThickness = 0.0f;
		dissolveData_->padding[0] = dissolveData_->padding[1] = 0.0f;
		dissolveData_->edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		shadowParameterData_->lightViewProjection = Matrix4x4::MakeIdentity();
		shadowParameterData_->shadowBias = 0.0f;
		shadowParameterData_->normalBias = 0.0f;
		shadowParameterData_->shadowStrength = 0.0f;
		shadowParameterData_->shadowMode = 0;
		shadowParameterData_->shadowDebugMode = 0;
		shadowParameterData_->padding[0] = 0.0f;
		initialized_ = true;
	}

	void InstancedObject3DRenderer::Finalize()
	{
		if (instanceSrvIndex_ != UINT32_MAX)
		{
			SRVManager::GetInstance()->Free(instanceSrvIndex_);
			instanceSrvIndex_ = UINT32_MAX;
		}
		instanceResource_.Reset();
		perViewResource_.Reset();
		cameraResource_.Reset();
		dissolveResource_.Reset();
		shadowParameterResource_.Reset();
		mappedInstances_ = nullptr;
		perViewData_ = nullptr;
		cameraData_ = nullptr;
		dissolveData_ = nullptr;
		shadowParameterData_ = nullptr;
		model_.reset();
		materialTextureSlots_.Clear();
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		maxInstanceCount_ = 0;
		instanceCount_ = 0;
		sourceInstances_.clear();
		instanceBufferDirty_ = false;
		frustumCullingEnabled_ = false;
		estimatedDrawIndexCount_ = 0;
		drawSkippedByBudget_ = false;
		dxCommon_ = nullptr;
		initialized_ = false;
	}

	uint64_t InstancedObject3DRenderer::GetModelTotalIndexCount() const
	{
		return model_ ? model_->GetTotalIndexCount() : 0ull;
	}

	bool InstancedObject3DRenderer::SetInstances(const std::vector<InstanceData>& instances)
	{
		if (!initialized_ || instances.size() > maxInstanceCount_) { return false; }
		sourceInstances_ = instances;
		instanceBufferDirty_ = true;
		if (frustumCullingEnabled_) { instanceCount_ = 0; }
		if (!frustumCullingEnabled_)
		{
			std::copy(sourceInstances_.begin(), sourceInstances_.end(), mappedInstances_);
			instanceCount_ = sourceInstances_.size();
			instanceBufferDirty_ = false;
		}
		return true;
	}

	bool InstancedObject3DRenderer::SetWorldMatrices(const std::vector<Matrix4x4>& worldMatrices, const Vector4& color)
	{
		if (!initialized_ || worldMatrices.size() > maxInstanceCount_) { return false; }
		std::vector<InstanceData> instances(worldMatrices.size());
		for (size_t i = 0; i < worldMatrices.size(); ++i)
		{
			instances[i].world = worldMatrices[i];
			instances[i].worldInverseTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(worldMatrices[i]));
			instances[i].color = color;
		}
		return SetInstances(instances);
	}

	bool InstancedObject3DRenderer::SetTransforms(const std::vector<InstanceTransform>& transforms)
	{
		if (!initialized_ || transforms.size() > maxInstanceCount_) { return false; }
		std::vector<InstanceData> instances;
		instances.reserve(transforms.size());
		for (const auto& transform : transforms)
		{
			InstanceData instance{};
			instance.world = Matrix4x4::MakeAffineMatrix(transform.scale, transform.rotation, transform.position);
			instance.worldInverseTranspose = Matrix4x4::Transpose(Matrix4x4::Inverse(instance.world));
			instance.color = transform.color;
			instances.push_back(instance);
		}
		return SetInstances(instances);
	}

	void InstancedObject3DRenderer::ApplyMaterialDesc(const MaterialDesc& desc)
	{
		if (!initialized_)
		{
			return; // GPU Material未初期化時はTextureロードを含む反映処理を行わない。
		}
		material_.ApplyDesc(desc); // 全インスタンスで共有する既存MaterialCBDataへBinding結果を反映する。
		materialTextureSlots_.ApplyDesc(desc);
		RestoreModelMaterials();

		if (materialTextureSlots_.HasBaseColorOverride())
		{
			for (D3D12_GPU_DESCRIPTOR_HANDLE& baseColor : materialSRVs_)
			{
				baseColor = materialTextureSlots_.ResolveBaseColor(baseColor); // BaseColor Overrideを全SubMesh/Instanceへ適用する。
			}
		}
		materialUsePointSampling_.assign(materialSRVs_.size(), desc.legacy.usePointSampling);
	}

	void InstancedObject3DRenderer::ResetMaterialBinding()
	{
		if (!initialized_)
		{
			return;
		}
		material_.ResetToDefault(); // Binding解除時は既存Forwardの既定値へ戻す。
		materialTextureSlots_.Reset();
		RestoreModelMaterials();
	}

	void InstancedObject3DRenderer::RestoreModelMaterials()
	{
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		if (!model_)
		{
			return;
		}

		materialSRVs_ = model_->GetMaterialSRVs();
		materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();
	}

	void InstancedObject3DRenderer::SetFrustumCullingEnabled(bool enabled)
	{
		if (frustumCullingEnabled_ == enabled) { return; }
		frustumCullingEnabled_ = enabled;
		instanceBufferDirty_ = true;
	}

	size_t InstancedObject3DRenderer::UploadSourceInstancesForEditorPicking()
	{
		if (!initialized_ || !dxCommon_ || !model_ || !mappedInstances_ || sourceInstances_.empty()) return 0;
		const size_t count = std::min(sourceInstances_.size(), maxInstanceCount_);
		std::copy_n(sourceInstances_.begin(), count, mappedInstances_);
		instanceCount_ = count;
		return count; // カリングで順番を詰め替えず、SV_InstanceIDと個別編集Indexを一致させる。
	}

	void InstancedObject3DRenderer::UpdateVisibleInstances(const Matrix4x4& viewProjection)
	{
		if (!frustumCullingEnabled_)
		{
			if (instanceBufferDirty_)
			{
				std::copy(sourceInstances_.begin(), sourceInstances_.end(), mappedInstances_);
				instanceCount_ = sourceInstances_.size();
				instanceBufferDirty_ = false;
			}
			return;
		}

		Frustum frustum;
		frustum.BuildFromViewProjection(viewProjection);
		instanceCount_ = 0;
		const bool hasBounds = model_ && model_->HasLocalBounds();
		const BoundingSphere localBounds = hasBounds ? model_->GetLocalBounds() : BoundingSphere{};
		for (const auto& instance : sourceInstances_)
		{
			bool visible = true;
			if (hasBounds)
			{
				BoundingSphere worldBounds{};
				worldBounds.center = Vector3::Transform(localBounds.center, instance.world);
				const float scaleX = std::sqrt(instance.world.m[0][0] * instance.world.m[0][0] + instance.world.m[0][1] * instance.world.m[0][1] + instance.world.m[0][2] * instance.world.m[0][2]);
				const float scaleY = std::sqrt(instance.world.m[1][0] * instance.world.m[1][0] + instance.world.m[1][1] * instance.world.m[1][1] + instance.world.m[1][2] * instance.world.m[1][2]);
				const float scaleZ = std::sqrt(instance.world.m[2][0] * instance.world.m[2][0] + instance.world.m[2][1] * instance.world.m[2][1] + instance.world.m[2][2] * instance.world.m[2][2]);
				worldBounds.radius = localBounds.radius * std::max({ scaleX, scaleY, scaleZ });
				visible = frustum.Intersects(worldBounds);
			}
			if (visible)
			{
				mappedInstances_[instanceCount_++] = instance;
			}
		}
		instanceBufferDirty_ = false;
	}

	void InstancedObject3DRenderer::Draw()
	{
		if (!initialized_ || !model_ || sourceInstances_.empty()) { return; }

		perViewData_->viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		// CPU側でObject3Dを大量生成せず、可視なInstanceDataだけをGPUへ詰めてまとめて描画する。
		UpdateVisibleInstances(perViewData_->viewProjection);

		if (instanceCount_ == 0)
		{
			estimatedDrawIndexCount_ = 0;
			drawSkippedByBudget_ = false;
			return;
		}

		const uint64_t modelIndexCount = model_->GetTotalIndexCount();
		estimatedDrawIndexCount_ = modelIndexCount * static_cast<uint64_t>(instanceCount_);

		// 高ポリゴンモデルを大量インスタンス描画してGPUがTDRで停止しないようにする。
		if (debugIndexBudget_ > 0 && estimatedDrawIndexCount_ > debugIndexBudget_)
		{
			drawSkippedByBudget_ = true;
			return;
		}

		drawSkippedByBudget_ = false;

		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		cameraData_->x = cameraPosition.x;
		cameraData_->y = cameraPosition.y;
		cameraData_->z = cameraPosition.z;
		cameraData_->padding = 0.0f;
		const auto* lightManager = LightManager::GetInstance();
		shadowParameterData_->lightViewProjection = lightManager->BuildShadowLightViewProjection(cameraPosition);
		shadowParameterData_->shadowBias = lightManager->GetShadowBias();
		shadowParameterData_->normalBias = lightManager->GetNormalBias();
		shadowParameterData_->shadowStrength = lightManager->GetShadowStrength();
		shadowParameterData_->shadowMode = lightManager->GetShadowReceiverMode();
		shadowParameterData_->shadowDebugMode = lightManager->IsShadowMapDebugEnabled() ? 1u : (lightManager->IsShadowFactorDebugEnabled() ? 2u : 0u);
		material_.Update();

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		Object3DCommon::GetInstance()->SetInstancedRenderSetting();
		material_.SetPipeline(0);
		commandList->SetGraphicsRootConstantBufferView(1, perViewResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);
		commandList->SetGraphicsRootConstantBufferView(7, dissolveResource_->GetGPUVirtualAddress());
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 8, dissolveMaskHandle_);
		commandList->SetGraphicsRootConstantBufferView(9, shadowParameterResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(10, dxCommon_->GetShadowMapSrvHandleGPU());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(12, instanceSrvIndex_);
		materialTextureSlots_.BindAdditionalSlots(commandList, 13, 14, 15, 16); // t6:MR t7:Normal t8:AO t9:Emissive

		// 同一Modelの各サブメッシュを、全インスタンス分まとめて少数のDrawIndexedInstancedで描画する。
		auto& meshes = model_->GetMeshes();
		for (size_t i = 0; i < meshes.size(); ++i)
		{
			if (i < materialSRVs_.size())
			{
				TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 2, materialSRVs_[i]);
				material_.SetUsePointSampling(i < materialUsePointSampling_.size() ? materialUsePointSampling_[i] : false);
				material_.Update();
			}
			meshes[i].DrawInstanced(static_cast<UINT>(instanceCount_));
		}
	}
}
