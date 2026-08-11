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
		const uint32_t frameCount = (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount());
		instanceFrameBuffers_.resize(frameCount);

		auto* srvManager = SRVManager::GetInstance();
		for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
		{
			for (size_t streamIndex = 0; streamIndex < kInstanceStreamCount; ++streamIndex)
			{
				InstanceStreamBuffer& stream = instanceFrameBuffers_[frameIndex].streams[streamIndex];
				stream.resource = ResourceManager::CreateBufferResource(
					dxCommon_->GetDevice(),
					sizeof(InstanceData) * maxInstanceCount_);
				if (!stream.resource)
				{
					throw std::runtime_error("InstancedObject3DRenderer failed to create frame instance buffer");
				}

				const HRESULT mapResult = stream.resource->Map(0, nullptr, reinterpret_cast<void**>(&stream.mappedInstances));
				if (FAILED(mapResult) || !stream.mappedInstances)
				{
					throw std::runtime_error("InstancedObject3DRenderer failed to map frame instance buffer");
				}

				stream.srvIndex = srvManager->Allocate();
				srvManager->CreateSRVForStructureBuffer(
					stream.srvIndex,
					stream.resource.Get(),
					static_cast<UINT>(maxInstanceCount_),
					sizeof(InstanceData));
			}
		}

		material_.Initialize();
		materialTextureSlots_.Reset();
		RestoreModelMaterials(); // Binding未指定時は共有Modelが読み込んだMaterial Textureを使用する。

		TextureManager::GetInstance()->LoadTexture("SkyBox/skybox.dds");
		environmentMapHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("SkyBox/skybox.dds");
		TextureManager::GetInstance()->LoadTexture("Effects/Masks/noise.dds");
		dissolveMaskHandle_ = TextureManager::GetInstance()->GetSrvHandleGPU("Effects/Masks/noise.dds");

		dissolveData_.threshold = 1.0f;
		dissolveData_.edgeThickness = 0.0f;
		dissolveData_.padding[0] = dissolveData_.padding[1] = 0.0f;
		dissolveData_.edgeColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		shadowParameterData_.lightViewProjection = Matrix4x4::MakeIdentity();
		shadowParameterData_.shadowBias = 0.0f;
		shadowParameterData_.normalBias = 0.0f;
		shadowParameterData_.shadowStrength = 0.0f;
		shadowParameterData_.shadowMode = 0;
		shadowParameterData_.shadowDebugMode = 0;
		shadowParameterData_.padding[0] = 0.0f;
		initialized_ = true;
	}

	void InstancedObject3DRenderer::Finalize()
	{
		auto* srvManager = SRVManager::GetInstance();
		for (InstanceFrameBuffers& frame : instanceFrameBuffers_)
		{
			for (InstanceStreamBuffer& stream : frame.streams)
			{
				if (stream.resource && stream.mappedInstances)
				{
					stream.resource->Unmap(0, nullptr);
				}
				stream.mappedInstances = nullptr;
				stream.resource.Reset();
				if (stream.srvIndex != UINT32_MAX)
				{
					srvManager->Free(stream.srvIndex);
					stream.srvIndex = UINT32_MAX;
				}
			}
		}
		instanceFrameBuffers_.clear();
		model_.reset();
		materialTextureSlots_.Clear();
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		maxInstanceCount_ = 0;
		instanceCount_ = 0;
		sourceInstances_.clear();
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
		instanceCount_ = frustumCullingEnabled_ ? 0 : sourceInstances_.size();
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
	}

	uint32_t InstancedObject3DRenderer::GetCurrentFrameIndex() const
	{
		return dxCommon_ && dxCommon_->GetCommandManager()
			? dxCommon_->GetCommandManager()->GetCurrentFrameIndex()
			: 0u;
	}

	InstancedObject3DRenderer::InstanceStreamBuffer* InstancedObject3DRenderer::GetInstanceStream(InstanceStreamUsage usage)
	{
		if (instanceFrameBuffers_.empty()) return nullptr;
		const uint32_t frameIndex = GetCurrentFrameIndex() % static_cast<uint32_t>(instanceFrameBuffers_.size());
		const size_t streamIndex = static_cast<size_t>(usage);
		if (streamIndex >= kInstanceStreamCount) return nullptr;
		return &instanceFrameBuffers_[frameIndex].streams[streamIndex];
	}

	const InstancedObject3DRenderer::InstanceStreamBuffer* InstancedObject3DRenderer::GetInstanceStream(InstanceStreamUsage usage) const
	{
		if (instanceFrameBuffers_.empty()) return nullptr;
		const uint32_t frameIndex = GetCurrentFrameIndex() % static_cast<uint32_t>(instanceFrameBuffers_.size());
		const size_t streamIndex = static_cast<size_t>(usage);
		if (streamIndex >= kInstanceStreamCount) return nullptr;
		return &instanceFrameBuffers_[frameIndex].streams[streamIndex];
	}

	size_t InstancedObject3DRenderer::UploadSourceInstancesForEditorPicking()
	{
		InstanceStreamBuffer* stream = GetInstanceStream(InstanceStreamUsage::Picking);
		if (!initialized_ || !dxCommon_ || !model_ || !stream || !stream->mappedInstances || sourceInstances_.empty()) return 0;
		const size_t count = std::min(sourceInstances_.size(), maxInstanceCount_);
		std::copy_n(sourceInstances_.begin(), count, stream->mappedInstances);
		instanceCount_ = count;
		return count; // Picking専用Streamへ書き込み、Main/ShadowのGPU読み取り内容を上書きしない。
	}

	void InstancedObject3DRenderer::UpdateVisibleInstances(const Matrix4x4& viewProjection)
	{
		InstanceStreamBuffer* stream = GetInstanceStream(InstanceStreamUsage::Main);
		if (!stream || !stream->mappedInstances)
		{
			instanceCount_ = 0;
			return;
		}

		if (!frustumCullingEnabled_)
		{
			const size_t count = std::min(sourceInstances_.size(), maxInstanceCount_);
			std::copy_n(sourceInstances_.begin(), count, stream->mappedInstances);
			instanceCount_ = count;
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
			if (visible && instanceCount_ < maxInstanceCount_)
			{
				stream->mappedInstances[instanceCount_++] = instance;
			}
		}
	}

	void InstancedObject3DRenderer::Draw()
	{
		if (!initialized_ || !model_ || sourceInstances_.empty()) { return; }

		perViewData_.viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		// Main描画は現在FrameのMain専用Instance Streamだけへ可視データを書き込む。
		UpdateVisibleInstances(perViewData_.viewProjection);

		if (instanceCount_ == 0)
		{
			estimatedDrawIndexCount_ = 0;
			drawSkippedByBudget_ = false;
			return;
		}

		const uint64_t modelIndexCount = model_->GetTotalIndexCount();
		estimatedDrawIndexCount_ = modelIndexCount * static_cast<uint64_t>(instanceCount_);

		if (debugIndexBudget_ > 0 && estimatedDrawIndexCount_ > debugIndexBudget_)
		{
			drawSkippedByBudget_ = true;
			return; // 高ポリゴンモデルの極端なInstanced DrawによるTDRを防ぐ。
		}

		drawSkippedByBudget_ = false;

		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		cameraData_.x = cameraPosition.x;
		cameraData_.y = cameraPosition.y;
		cameraData_.z = cameraPosition.z;
		cameraData_.padding = 0.0f;
		const auto* lightManager = LightManager::GetInstance();
		shadowParameterData_.lightViewProjection = lightManager->BuildShadowLightViewProjection(cameraPosition);
		shadowParameterData_.shadowBias = lightManager->GetShadowBias();
		shadowParameterData_.normalBias = lightManager->GetNormalBias();
		shadowParameterData_.shadowStrength = lightManager->GetShadowStrength();
		shadowParameterData_.shadowMode = lightManager->GetShadowReceiverMode();
		shadowParameterData_.shadowDebugMode = lightManager->IsShadowMapDebugEnabled() ? 1u : (lightManager->IsShadowFactorDebugEnabled() ? 2u : 0u);
		material_.Update();

		FrameUploadArena& frameUploadArena = dxCommon_->GetFrameUploadArena();
		const FrameUploadArena::Allocation perViewAllocation = frameUploadArena.AllocateConstant(perViewData_);
		const FrameUploadArena::Allocation cameraAllocation = frameUploadArena.AllocateConstant(cameraData_);
		const FrameUploadArena::Allocation dissolveAllocation = frameUploadArena.AllocateConstant(dissolveData_);
		const FrameUploadArena::Allocation shadowParameterAllocation = frameUploadArena.AllocateConstant(shadowParameterData_);
		InstanceStreamBuffer* stream = GetInstanceStream(InstanceStreamUsage::Main);
		if (!stream || stream->srvIndex == UINT32_MAX ||
			!perViewAllocation.IsValid() || !cameraAllocation.IsValid() ||
			!dissolveAllocation.IsValid() || !shadowParameterAllocation.IsValid())
		{
			return;
		}

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		Object3DCommon::GetInstance()->SetInstancedRenderSetting();
		material_.SetPipeline(0);
		commandList->SetGraphicsRootConstantBufferView(1, perViewAllocation.gpuAddress);
		commandList->SetGraphicsRootConstantBufferView(3, cameraAllocation.gpuAddress);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4, environmentMapHandle_);
		commandList->SetGraphicsRootConstantBufferView(7, dissolveAllocation.gpuAddress);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 8, dissolveMaskHandle_);
		commandList->SetGraphicsRootConstantBufferView(9, shadowParameterAllocation.gpuAddress);
		commandList->SetGraphicsRootDescriptorTable(10, dxCommon_->GetShadowMapSrvHandleGPU());
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(12, stream->srvIndex);
		materialTextureSlots_.BindAdditionalSlots(commandList, 13, 14, 15, 16); // t6:MR t7:Normal t8:AO t9:Emissive

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
