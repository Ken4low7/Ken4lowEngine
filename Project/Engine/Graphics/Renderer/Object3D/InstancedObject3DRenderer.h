#pragma once
#include "DX12Include.h"
#include "Material.h"
#include "Matrix4x4.h"
#include "Vector4.h"
#include "Vector3.h"
#include "CameraManager.h"
#include "DirectXCommon.h"
#include "Model.h"
#include "ObjectIdPipeline.h"
#include "SRVManager.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Ken4lowEngine
{
	/// <summary>同じModelをGPUインスタンシングでまとめて描画する専用レンダラーです。</summary>
	class InstancedObject3DRenderer
	{
	public:
		struct InstanceData
		{
			Matrix4x4 world;
			Matrix4x4 worldInverseTranspose;
			Vector4 color;
		};

		struct InstanceTransform
		{
			Vector3 position{};
			Vector3 rotation{};
			Vector3 scale{ 1.0f, 1.0f, 1.0f };
			Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		};

		InstancedObject3DRenderer() = default;
		~InstancedObject3DRenderer();
		InstancedObject3DRenderer(const InstancedObject3DRenderer&) = delete;
		InstancedObject3DRenderer& operator=(const InstancedObject3DRenderer&) = delete;

		void Initialize(const std::string& modelPath, size_t maxInstanceCount = 30000);
		void Finalize();
		uint64_t GetModelTotalIndexCount() const;
		bool SetInstances(const std::vector<InstanceData>& instances);
		bool SetWorldMatrices(const std::vector<Matrix4x4>& worldMatrices, const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });
		bool SetTransforms(const std::vector<InstanceTransform>& transforms);
		void ApplyMaterialDesc(const MaterialDesc& desc);
		void ResetMaterialBinding();
		void Draw();
		void DrawShadow();

		/// <summary>全InstanceへbaseObjectId + SV_InstanceIDを書き込み、1Drawで個別Pickingします。</summary>
		void DrawEditorObjectId(uint32_t baseObjectId);
		
		/// <summary>選択輪郭用に指定Instanceだけを同じObject IDで描画します。</summary>
		void DrawEditorInstanceObjectId(size_t sourceInstanceIndex, uint32_t objectId);
		
		size_t GetInstanceCount() const { return sourceInstances_.size(); }
		size_t GetVisibleInstanceCount() const { return instanceCount_; }
		size_t GetMaxInstanceCount() const { return maxInstanceCount_; }
		void SetDebugIndexBudget(uint64_t budget) { debugIndexBudget_ = budget; }
		uint64_t GetEstimatedDrawIndexCount() const { return estimatedDrawIndexCount_; }
		bool WasDrawSkippedByBudget() const { return drawSkippedByBudget_; }
		void SetMaterialColor(const Vector4& color) { material_.SetColor(color); }
		void SetFrustumCullingEnabled(bool enabled);
		bool IsFrustumCullingEnabled() const { return frustumCullingEnabled_; }

	private:
		struct PerViewData { Matrix4x4 viewProjection; };
		struct CameraForGPU { float x, y, z, padding; };
		struct DissolveSetting
		{
			float threshold;
			float edgeThickness;
			float padding[2];
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

		size_t UploadSourceInstancesForEditorPicking();		

		DirectXCommon* dxCommon_ = nullptr;
		std::shared_ptr<Model> model_;
		Material material_{};
		MaterialTextureSlots materialTextureSlots_{};
		ComPtr<ID3D12Resource> instanceResource_;
		InstanceData* mappedInstances_ = nullptr;
		uint32_t instanceSrvIndex_ = UINT32_MAX;
		size_t maxInstanceCount_ = 0;
		size_t instanceCount_ = 0;
		ComPtr<ID3D12Resource> perViewResource_;
		PerViewData* perViewData_ = nullptr;
		ComPtr<ID3D12Resource> cameraResource_;
		CameraForGPU* cameraData_ = nullptr;
		ComPtr<ID3D12Resource> dissolveResource_;
		DissolveSetting* dissolveData_ = nullptr;
		ComPtr<ID3D12Resource> shadowParameterResource_;
		ShadowParameterForGPU* shadowParameterData_ = nullptr;
		std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> materialSRVs_;
		std::vector<bool> materialUsePointSampling_;
		D3D12_GPU_DESCRIPTOR_HANDLE environmentMapHandle_{};
		D3D12_GPU_DESCRIPTOR_HANDLE dissolveMaskHandle_{};
		bool initialized_ = false;
		std::vector<InstanceData> sourceInstances_{};
		bool instanceBufferDirty_ = false;
		bool frustumCullingEnabled_ = false;
		uint64_t debugIndexBudget_ = 50'000'000ull;
		uint64_t estimatedDrawIndexCount_ = 0;
		bool drawSkippedByBudget_ = false;

		void UpdateVisibleInstances(const Matrix4x4& viewProjection);
		void RestoreModelMaterials();
	};
}

#include "InstancedObject3DRendererShadow.inl"
