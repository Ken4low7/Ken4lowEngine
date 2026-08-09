#pragma once

#include "DX12Include.h"
#include "LightManager.h"
#include "Matrix4x4.h"
#include "Vector4.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class ShadowMapArrayRenderTarget;

	/// <summary>
	/// 既存b4 ShadowParameterを変更せず、選択ライトのFrame共通行列とPoint Cube / CSM情報をb6へ追加するGPU契約です。
	/// </summary>
	struct ExtendedShadowParameterGPU
	{
		std::array<Matrix4x4, 4> cascadeLightViewProjection{}; // [0]はLegacy Directional/SpotでもFrame共通行列として使用する。
		Vector4 cascadeSplits{};
		Vector4 pointLightPositionAndFar{}; // Local Light時はPosition/Far、CSM時はCamera Forward/MaxDistance。
		Vector4 cameraPositionAndPointNear{};
		uint32_t shadowTechnique = 0; // 0:Off 1:Directional 2:SpotLinear 3:PointCube 4:CSM
		uint32_t cascadeCount = 0;
		uint32_t shadowCasterLightIndex = UINT32_MAX; // GPU転送後のPunctualLight index
		uint32_t padding0 = 0;
		float shadowBias = 0.0f;
		float normalBias = 0.025f;
		float shadowStrength = 0.6f;
		float padding1 = 0.0f;
	};
	static_assert(sizeof(ExtendedShadowParameterGPU) == 336, "ExtendedShadowParameterGPU must match HLSL b6 layout");

	/// <summary>Shadowの行列生成、複数Slice描画、追加GPUリソースのバインドを一箇所で管理します。</summary>
	class ShadowSystem
	{
	public:
		ShadowSystem();
		~ShadowSystem();
		static constexpr uint32_t kCascadeCount = 4;
		static constexpr uint32_t kPointFaceCount = 6;

		void Initialize(DirectXCommon* dxCommon, uint32_t shadowMapSize);
		void Finalize();
		void Resize(uint32_t shadowMapSize);

		/// <summary>選択中ライトに応じ、Directional/Spotを1回、Pointを6回、CSMを4回描画します。</summary>
		void Execute(LightManager& lightManager, const std::function<void()>& drawShadowObjects);

		/// <summary>b6、t10、t11を現在のRootSignatureへバインドします。</summary>
		void Bind(uint32_t extendedShadowCbvRootIndex, uint32_t csmSrvRootIndex, uint32_t pointSrvRootIndex) const;

		const Matrix4x4& GetActivePassLightViewProjection() const { return activePassLightViewProjection_; }
		const ExtendedShadowParameterGPU& GetGpuData() const { return *gpuData_; }

	private:
		void ExecuteLegacyPass(LightManager& lightManager, const std::function<void()>& drawShadowObjects);
		void ExecutePointPass(LightManager& lightManager, const LightManager::PunctualLightGPU& light, uint32_t gpuLightIndex, const std::function<void()>& drawShadowObjects);
		void ExecuteCsmPass(LightManager& lightManager, const LightManager::PunctualLightGPU& light, uint32_t gpuLightIndex, const std::function<void()>& drawShadowObjects);
		uint32_t ResolveGpuLightIndex(const LightManager& lightManager, int32_t legacyLightIndex) const;
		Matrix4x4 BuildStableDirectionalMatrix(const Vector3& direction, const Vector3& focus, float halfWidth, float halfHeight, float distance, float nearZ, float farZ, uint32_t mapSize) const;
		void ResetGpuData(const LightManager& lightManager);

		DirectXCommon* dxCommon_ = nullptr;
		std::unique_ptr<ShadowMapArrayRenderTarget> csmRenderTarget_;
		std::unique_ptr<ShadowMapArrayRenderTarget> pointRenderTarget_;
		ComPtr<ID3D12Resource> extendedShadowResource_;
		ExtendedShadowParameterGPU* gpuData_ = nullptr;
		Matrix4x4 activePassLightViewProjection_ = Matrix4x4::MakeIdentity();
		uint32_t desiredMapSize_ = 2048;
	};
}