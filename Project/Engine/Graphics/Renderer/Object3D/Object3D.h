#pragma once
#include "DX12Include.h"
#include "WorldTransform.h"
#include "TextureManager.h"
#include "Material.h"
#include "VertexData.h"
#include "Camera.h"
#include "TransformationMatrix.h"
#include "Engine/Graphics/Culling/BoundingVolume.h"
#include "Model.h"
#include "ObjectIdPipeline.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <numbers>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class Object3DCommon;
	class SkyBox;

	class Object3D
	{
	public:
		struct CameraForGPU
		{
			Vector3 worldPosition;
		};

		struct DissolveSetting
		{
			float threshold;
			float edgeThickness;
			float padding0[2];
			Vector4 edgeColor;
		};

		struct ShadowParameterForGPU
		{
			Matrix4x4 lightViewProjection;
			float shadowBias;
			float normalBias;
			float shadowStrength;
			uint32_t shadowMode;
			uint32_t shadowDebugMode;
			float padding[1];
		};

		void Initialize(const std::string& fileName);
		void Update();
		void UpdateWithWorldMatrix(const Matrix4x4& worldMatrix);
		void UpdateShadowMatrix(const Matrix4x4& lightViewProjection);
		void DrawImGui();
		void Draw();
		void DrawMeshes(const std::vector<size_t>& meshIndices);
		void DrawShadow();

		void DrawEditorObjectId(uint32_t objectId)
		{
			if (!dxCommon_ || !model_ || objectId == 0)
			{
				return;
			}

			ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
			ObjectIdPipeline::GetInstance()->BindStatic(commandList, objectId);
			worldTransform_.SetPipeline(0);
			for (auto& mesh : model_->GetMeshes())
			{
				mesh.Draw();
			}
		}

		void SetModel(const std::string& filePath);
		void SetScale(const Vector3& scale) { worldTransform_.scale_ = scale; }
		Vector3 GetScale() const { return worldTransform_.scale_; }
		void SetRotate(const Vector3& rotate) { worldTransform_.rotate_ = rotate; }
		Vector3 GetRotate() const { return worldTransform_.rotate_; }
		void SetTranslate(const Vector3& translate) { worldTransform_.translate_ = translate; }
		Vector3 GetTranslate() const { return worldTransform_.translate_; }
		void SetColor(const Vector4& color) { material_.SetColor(color); }
		void SetPbrEnabled(bool enabled) { material_.SetPbrEnabled(enabled); }
		void SetMetallic(float metallic) { material_.SetMetallic(metallic); }
		void SetRoughness(float roughness) { material_.SetRoughness(roughness); }
		void SetEmissiveFactor(const Vector4& emissiveFactor) { material_.SetEmissiveFactor(emissiveFactor); } // ゲーム側の脈動演出からObject3Dの発光量を安全に変更する。
		void SetCamera(Camera* camera) { camera_ = camera; }
		void SetReflectivity(float reflectivity) { material_.SetReflection(reflectivity); }
		void ApplyMaterialDesc(const MaterialDesc& desc);
		void ResetMaterialBinding();
		void SetTextureForAll(const std::string& texturePath);
		void SetTextureForSubmesh(size_t index, const std::string& texturePath);
		size_t GetSubmeshCount() const;
		BoundingSphere GetWorldBoundsForCulling() const { return GetWorldBounds(); }
		BoundingSphere GetMeshWorldBoundsForCulling(size_t meshIndex) const { return GetMeshWorldBounds(meshIndex); }
		bool HasMeshWorldBoundsForCulling(size_t meshIndex) const { return HasMeshWorldBounds(meshIndex); }
		void SetFrustumCullingEnabled(bool enabled) { frustumCullingEnabled_ = enabled; }
		bool IsFrustumCullingEnabled() const { return frustumCullingEnabled_; }
		void SetStageObjectCullingUnit(bool enabled) { isStageObjectCullingUnit_ = enabled; }
		bool IsStageObjectCullingUnit() const { return isStageObjectCullingUnit_; }
		void SetIgnoreStageChunkCulling(bool ignore) { ignoreStageChunkCulling_ = ignore; }
		bool IsIgnoreStageChunkCulling() const { return ignoreStageChunkCulling_; }
		bool HasWorldBoundsForCulling() const { return HasWorldBounds(); }
		void SetAlphaBlendEnabled(bool enabled) { alphaBlendEnabled_ = enabled; }
		bool IsAlphaBlendEnabled() const { return alphaBlendEnabled_; }

		void SetDissolveThreshold(float threshold) { dissolveSetting_->threshold = threshold; }
		void SetDissolveEdgeThickness(float thickness) { dissolveSetting_->edgeThickness = thickness; }
		void SetDissolveEdgeColor(const Vector4& color) { dissolveSetting_->edgeColor = color; }

	private:
		void InitializeCameraResource();
		void InitializeDissolveResource();
		void InitializeShadowResource();
		void InitializeShadowParameterResource();
		void AcquireShadowMapHandle();
		BoundingSphere GetWorldBounds() const;
		BoundingSphere GetMeshWorldBounds(size_t meshIndex) const;
		BoundingSphere TransformLocalBounds(const BoundingSphere& localBounds) const;
		bool HasWorldBounds() const;
		bool HasMeshWorldBounds(size_t meshIndex) const;
		void DrawInternal(const std::vector<size_t>* meshIndices);
		void DrawBoundsDebug(const BoundingSphere& bounds, bool visible) const;

	private:
		DirectXCommon* dxCommon_ = nullptr;
		Camera* camera_ = nullptr;
		SkyBox* skyBox_ = nullptr;
		std::shared_ptr<Model> model_;
		Material material_;
		MaterialTextureSlots materialTextureSlots_{};
		WorldTransform worldTransform_;
		WorldTransform shadowWorldTransform_;
		ComPtr<ID3D12Resource> cameraResource;
		CameraForGPU* cameraData = nullptr;
		ComPtr<ID3D12Resource> shadowTransformResource_;
		TransformationMatrix* shadowTransformData_ = nullptr;
		float alpha = 1.0f;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
		std::vector<bool> materialUsePointSampling_;
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};
		D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};
		Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
		DissolveSetting* dissolveSetting_ = nullptr;
		ComPtr<ID3D12Resource> shadowParameterResource_;
		ShadowParameterForGPU* shadowParameterData_ = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE shadowMapHandle_{};
		bool frustumCullingEnabled_ = true;
		bool isStageObjectCullingUnit_ = false;
		bool ignoreStageChunkCulling_ = false;
		bool alphaBlendEnabled_ = false;
	};
} // namespace Ken4lowEngine
