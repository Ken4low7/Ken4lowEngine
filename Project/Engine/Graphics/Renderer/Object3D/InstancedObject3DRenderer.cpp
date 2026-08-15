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
#include "Engine/Graphics/Renderer/Environment/EnvironmentMapManager.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
				stream.resource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(InstanceData) * maxInstanceCount_);
				if (!stream.resource) throw std::runtime_error("InstancedObject3DRenderer failed to create frame instance buffer");
				const HRESULT mapResult = stream.resource->Map(0, nullptr, reinterpret_cast<void**>(&stream.mappedInstances));
				if (FAILED(mapResult) || !stream.mappedInstances) throw std::runtime_error("InstancedObject3DRenderer failed to map frame instance buffer");
				stream.srvIndex = srvManager->Allocate();
				srvManager->CreateSRVForStructureBuffer(stream.srvIndex, stream.resource.Get(), static_cast<UINT>(maxInstanceCount_), sizeof(InstanceData));
			}
		}

		material_.Initialize();
		materialTextureSlots_.Reset();
		RestoreModelMaterials(); // Binding未指定時は共有Modelが読み込んだMaterial Textureを使用する。

		EnvironmentMapManager::GetInstance()->GetEnvironmentMapHandle(); // InstancedもScene共通Cubemapを使用する。
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
				if (stream.resource && stream.mappedInstances) stream.resource->Unmap(0, nullptr);
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
		visibleInstanceScratch_.clear();
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

	float InstancedObject3DRenderer::CalculateForwardSortDepth() const
	{
		if (sourceInstances_.empty()) return 0.0f;
		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const Vector3 cameraForward = CameraManager::GetInstance()->GetActiveCameraForward();
		const bool backToFront = material_.GetBlendMode() == MaterialBlendMode::Transparent || material_.GetBlendMode() == MaterialBlendMode::Additive;
		float selectedDepth = backToFront ? -std::numeric_limits<float>::infinity() : std::numeric_limits<float>::infinity();
		const Vector3 localCenter = model_ && model_->HasLocalBounds() ? model_->GetLocalBounds().center : Vector3{};
		for (const InstanceData& instance : sourceInstances_)
		{
			const Vector3 center = Vector3::Transform(localCenter, instance.world);
			const Vector3 toInstance = { center.x - cameraPosition.x, center.y - cameraPosition.y, center.z - cameraPosition.z };
			const float depth = toInstance.x * cameraForward.x + toInstance.y * cameraForward.y + toInstance.z * cameraForward.z;
			selectedDepth = backToFront ? std::max(selectedDepth, depth) : std::min(selectedDepth, depth);
		}
		return std::isfinite(selectedDepth) ? selectedDepth : 0.0f;
	}

	bool InstancedObject3DRenderer::SetInstances(const std::vector<InstanceData>& instances)
	{
		if (!initialized_ || instances.size() > maxInstanceCount_) return false;
		sourceInstances_ = instances;
		instanceCount_ = frustumCullingEnabled_ ? 0 : sourceInstances_.size();
		return true;
	}

	bool InstancedObject3DRenderer::SetWorldMatrices(const std::vector<Matrix4x4>& worldMatrices, const Vector4& color)
	{
		if (!initialized_ || worldMatrices.size() > maxInstanceCount_) return false;
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
		if (!initialized_ || transforms.size() > maxInstanceCount_) return false;
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
		if (!initialized_) return; // GPU Material未初期化時はTextureロードを含む反映処理を行わない。
		material_.ApplyDesc(desc);
		materialTextureSlots_.ApplyDesc(desc);
		RestoreModelMaterials();
		if (materialTextureSlots_.HasBaseColorOverride())
		{
			for (D3D12_GPU_DESCRIPTOR_HANDLE& baseColor : materialSRVs_) baseColor = materialTextureSlots_.ResolveBaseColor(baseColor);
		}
		materialUsePointSampling_.assign(materialSRVs_.size(), desc.legacy.usePointSampling);
	}

	void InstancedObject3DRenderer::ResetMaterialBinding()
	{
		if (!initialized_) return;
		material_.ResetToDefault();
		materialTextureSlots_.Reset();
		RestoreModelMaterials();
	}

	void InstancedObject3DRenderer::RestoreModelMaterials()
	{
		materialSRVs_.clear();
		materialUsePointSampling_.clear();
		if (!model_) return;
		materialSRVs_ = model_->GetMaterialSRVs();
		materialUsePointSampling_ = model_->GetMaterialPointSamplingFlags();
	}

	void InstancedObject3DRenderer::SetFrustumCullingEnabled(bool enabled)
	{
		if (frustumCullingEnabled_ == enabled) return;
		frustumCullingEnabled_ = enabled;
	}

	MaterialCullMode InstancedObject3DRenderer::ResolveEffectiveCullMode() const
	{
		const MaterialCullMode baseCullMode = material_.GetCullMode();
		if (baseCullMode == MaterialCullMode::None || sourceInstances_.empty()) return baseCullMode;

		bool hasNormalHandedness = false;
		bool hasMirroredHandedness = false;
		for (const InstanceData& instance : sourceInstances_)
		{
			if (CalculateWorldHandednessDeterminant(instance.world) < 0.0f) hasMirroredHandedness = true;
			else hasNormalHandedness = true;
			if (hasNormalHandedness && hasMirroredHandedness)
			{
				return MaterialCullMode::None; // 1 Drawに両方の巻き順が混在する場合は欠損を避けるため両面描画へ退避する。
			}
		}

		if (hasMirroredHandedness)
		{
			return baseCullMode == MaterialCullMode::Back ? MaterialCullMode::Front : MaterialCullMode::Back;
		}
		return baseCullMode;
	}

	uint32_t InstancedObject3DRenderer::GetCurrentFrameIndex() const
	{
		return dxCommon_ && dxCommon_->GetCommandManager() ? dxCommon_->GetCommandManager()->GetCurrentFrameIndex() : 0u;
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

		const MaterialBlendMode blendMode = material_.GetBlendMode();
		const bool backToFront = blendMode == MaterialBlendMode::Transparent || blendMode == MaterialBlendMode::Additive;
		visibleInstanceScratch_.clear();
		if (backToFront) visibleInstanceScratch_.reserve(std::min(sourceInstances_.size(), maxInstanceCount_));

		if (!frustumCullingEnabled_ && !backToFront)
		{
			const size_t count = std::min(sourceInstances_.size(), maxInstanceCount_);
			std::copy_n(sourceInstances_.begin(), count, stream->mappedInstances);
			instanceCount_ = count;
			return;
		}

		const bool hasBounds = model_ && model_->HasLocalBounds();
		const BoundingSphere localBounds = hasBounds ? model_->GetLocalBounds() : BoundingSphere{};
		Frustum frustum;
		if (frustumCullingEnabled_) frustum.BuildFromViewProjection(viewProjection);
		instanceCount_ = 0;

		for (const InstanceData& instance : sourceInstances_)
		{
			bool visible = true;
			if (frustumCullingEnabled_ && hasBounds)
			{
				BoundingSphere worldBounds{};
				worldBounds.center = Vector3::Transform(localBounds.center, instance.world);
				const float scaleX = std::sqrt(instance.world.m[0][0] * instance.world.m[0][0] + instance.world.m[0][1] * instance.world.m[0][1] + instance.world.m[0][2] * instance.world.m[0][2]);
				const float scaleY = std::sqrt(instance.world.m[1][0] * instance.world.m[1][0] + instance.world.m[1][1] * instance.world.m[1][1] + instance.world.m[1][2] * instance.world.m[1][2]);
				const float scaleZ = std::sqrt(instance.world.m[2][0] * instance.world.m[2][0] + instance.world.m[2][1] * instance.world.m[2][1] + instance.world.m[2][2] * instance.world.m[2][2]);
				worldBounds.radius = localBounds.radius * std::max({ scaleX, scaleY, scaleZ });
				visible = frustum.Intersects(worldBounds);
			}
			if (!visible) continue;

			if (backToFront)
			{
				if (visibleInstanceScratch_.size() < maxInstanceCount_) visibleInstanceScratch_.push_back(instance);
			}
			else if (instanceCount_ < maxInstanceCount_)
			{
				stream->mappedInstances[instanceCount_++] = instance;
			}
		}

		if (!backToFront) return;

		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		const Vector3 cameraForward = CameraManager::GetInstance()->GetActiveCameraForward();
		const Vector3 localCenter = hasBounds ? localBounds.center : Vector3{};
		std::stable_sort(visibleInstanceScratch_.begin(), visibleInstanceScratch_.end(),
			[&](const InstanceData& lhs, const InstanceData& rhs)
			{
				const Vector3 lhsCenter = Vector3::Transform(localCenter, lhs.world);
				const Vector3 rhsCenter = Vector3::Transform(localCenter, rhs.world);
				const Vector3 lhsOffset = { lhsCenter.x - cameraPosition.x, lhsCenter.y - cameraPosition.y, lhsCenter.z - cameraPosition.z };
				const Vector3 rhsOffset = { rhsCenter.x - cameraPosition.x, rhsCenter.y - cameraPosition.y, rhsCenter.z - cameraPosition.z };
				const float lhsDepth = lhsOffset.x * cameraForward.x + lhsOffset.y * cameraForward.y + lhsOffset.z * cameraForward.z;
				const float rhsDepth = rhsOffset.x * cameraForward.x + rhsOffset.y * cameraForward.y + rhsOffset.z * cameraForward.z;
				return lhsDepth > rhsDepth; // Main Streamだけを奥から手前へ並べ、編集用sourceInstances_のID順は保持する。
			});

		instanceCount_ = std::min(visibleInstanceScratch_.size(), maxInstanceCount_);
		std::copy_n(visibleInstanceScratch_.begin(), instanceCount_, stream->mappedInstances);
	}

	void InstancedObject3DRenderer::Draw()
	{
		if (!initialized_ || !model_ || sourceInstances_.empty()) return;

		perViewData_.viewProjection = CameraManager::GetInstance()->GetActiveViewProjectionMatrix();
		UpdateVisibleInstances(perViewData_.viewProjection); // Main描画は現在FrameのMain専用Instance Streamだけへ可視データを書き込む。
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
		if (!stream || stream->srvIndex == UINT32_MAX || !perViewAllocation.IsValid() || !cameraAllocation.IsValid() || !dissolveAllocation.IsValid() || !shadowParameterAllocation.IsValid()) return;

		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		SRVManager::GetInstance()->PreDraw();
		const MaterialCullMode cullMode = ResolveEffectiveCullMode();
		switch (material_.GetBlendMode())
		{
		case MaterialBlendMode::Transparent:
			Object3DCommon::GetInstance()->SetInstancedAlphaRenderSetting(cullMode);
			break;
		case MaterialBlendMode::Additive:
			Object3DCommon::GetInstance()->SetInstancedAdditiveRenderSetting(cullMode);
			break;
		case MaterialBlendMode::Masked:
		case MaterialBlendMode::Opaque:
		default:
			Object3DCommon::GetInstance()->SetInstancedRenderSetting(cullMode);
			break;
		}
		material_.SetPipeline(0);
		commandList->SetGraphicsRootConstantBufferView(1, perViewAllocation.gpuAddress);
		commandList->SetGraphicsRootConstantBufferView(3, cameraAllocation.gpuAddress);
		TextureManager::GetInstance()->SetGraphicsRootDescriptorTable(commandList, 4,
			EnvironmentMapManager::GetInstance()->GetEnvironmentMapHandle()); // Instancedも現在SceneのEnvironmentをDraw時に参照する。
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
