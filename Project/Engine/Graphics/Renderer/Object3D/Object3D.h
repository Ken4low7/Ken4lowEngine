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

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <numbers>
#include <limits>

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
			if (!dxCommon_ || !model_ || objectId == 0) return;
			ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
			auto& meshes = model_->GetMeshes();
			std::array<std::vector<size_t>, 3> meshGroups{};
			for (size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
			{
				meshGroups[static_cast<size_t>(ResolveSubmeshCullMode(meshIndex))].push_back(meshIndex);
			}

			for (size_t groupIndex = 0; groupIndex < meshGroups.size(); ++groupIndex)
			{
				if (meshGroups[groupIndex].empty()) continue;
				const MaterialCullMode cullMode = static_cast<MaterialCullMode>(groupIndex);
				ObjectIdPipeline::GetInstance()->BindStatic(commandList, objectId, cullMode); // PickingもSubMeshごとの実際の表示面だけを選択対象にする。
				worldTransform_.SetPipeline(0);
				for (const size_t meshIndex : meshGroups[groupIndex]) meshes[meshIndex].Draw();
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
		void SetCullMode(MaterialCullMode cullMode) { material_.SetCullMode(cullMode); materialCullOverrideEnabled_ = true; }
		MaterialCullMode GetCullMode() const { return material_.GetCullMode(); }
		MaterialBlendMode GetBlendMode() const { return material_.GetBlendMode(); } // Forward QueueはGPU定数ではなくMaterial分類だけを参照する。
		void SetCamera(Camera* camera) { camera_ = camera; }
		void SetReflectivity(float reflectivity) { material_.SetReflection(reflectivity); }
		void SetWaterSurfaceState(
			bool enabled,
			float time,
			float waveScale,
			float waveSpeed,
			float normalStrength,
			float fresnelF0,
			float reflectionDistortion,
			float secondaryWaveScale)
		{
			material_.SetWaterSurfaceState(enabled, time, waveScale, waveSpeed, normalStrength, fresnelF0, reflectionDistortion, secondaryWaveScale); // Water Componentから描画内部Materialへ専用状態だけを転送する。
		}
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
		bool TryGetSupportPointAlongWorldDirection(const Vector3& worldDirection, Vector3& outPoint) const
		{
			if (!model_) return false;
			const Vector3 direction = Vector3::NormalizeSafe(worldDirection, { 0.0f, 1.0f, 0.0f });
			const Matrix4x4& world = worldTransform_.GetWorldMatrix();
			float bestProjection = std::numeric_limits<float>::lowest();
			bool found = false;
			for (const SubMesh& subMesh : model_->GetModelData().subMeshes)
			{
				for (const VertexData& vertex : subMesh.vertices)
				{
					const Vector3 localPosition{ vertex.position.x, vertex.position.y, vertex.position.z };
					const Vector3 worldPosition = Vector3::Transform(localPosition, world);
					const float projection = Vector3::Dot(worldPosition, direction);
					if (!found || projection > bestProjection)
					{
						bestProjection = projection;
						outPoint = worldPosition;
						found = true;
					}
				}
			}
			return found; // 鏡面Auto Fitは境界球ではなく実頂点の最外面を使い、Cube上面などへ正確に合わせる。
		}
		void SetAlphaBlendEnabled(bool enabled)
		{
			alphaBlendEnabled_ = enabled;
			if (enabled)
			{
				material_.SetBlendMode(MaterialBlendMode::Transparent); // Legacy Alpha指定もForward Queueが理解できるMaterial分類へ同期する。
			}
			else if (material_.GetBlendMode() == MaterialBlendMode::Transparent)
			{
				material_.SetBlendMode(MaterialBlendMode::Opaque);
			}
		}
		bool IsAlphaBlendEnabled() const { return alphaBlendEnabled_; }

		void SetDissolveThreshold(float threshold) { dissolveSetting_.threshold = threshold; }
		void SetDissolveEdgeThickness(float thickness) { dissolveSetting_.edgeThickness = thickness; }
		void SetDissolveEdgeColor(const Vector4& color) { dissolveSetting_.edgeColor = color; }

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
		MaterialCullMode ResolveSubmeshCullMode(size_t meshIndex) const;
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
		CameraForGPU cameraData_{};
		TransformationMatrix shadowTransformData_{};
		float alpha = 1.0f;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
		std::vector<bool> materialUsePointSampling_;
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};
		D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};
		DissolveSetting dissolveSetting_{};
		ShadowParameterForGPU shadowParameterData_{}; // GPUへ渡す直前までCPU stagingに保持し、Frame Upload Arenaへコピーする。
		D3D12_GPU_DESCRIPTOR_HANDLE shadowMapHandle_{};
		bool materialCullOverrideEnabled_ = false; // 明示指定がない間はImportされたSubMeshのdoubleSided/Cull情報を優先する。
		bool frustumCullingEnabled_ = true;
		bool isStageObjectCullingUnit_ = false;
		bool ignoreStageChunkCulling_ = false;
		bool alphaBlendEnabled_ = false;
	};
} // namespace Ken4lowEngine