#pragma once
#include "DX12Include.h"
#include "LightManager.h"
#include "Camera.h"
#include "Object3DPipelineSet.h"
#include "InstancedObject3DPipelineSet.h"
#include "ShadowCasterPipelineSet.h"
#include "Vector4.h"
#include "Engine/Graphics/Culling/FrustumCullingSystem.h"

namespace Ken4lowEngine
{
	class DirectXCommon;
	class PipelineFactory;

	class Object3DCommon
	{
	public:
		enum class CullingCameraMode
		{
			MainCamera,
			DebugCamera,
			ActiveCamera
		};

	public:
		static Object3DCommon* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();
		void BeginObject3DPass();
		void DrawImGui();

	public: /// ---------- 描画設定関数 ---------- ///

		void SetRenderSetting();
		void SetAlphaRenderSetting();
		void SetInstancedRenderSetting();
		void SetShadowMapRenderSetting();
		void SetInstancedShadowMapRenderSetting();
		bool ShouldDrawObject(const BoundingSphere& worldBounds, bool objectCullingEnabled, bool hasBounds, bool isStageObject = false);
		bool ShouldDrawMesh(const BoundingSphere& worldBounds, bool objectCullingEnabled, bool hasBounds);
		FrustumCullingSystem& GetFrustumCullingSystem() { return frustumCullingSystem_; }
		const FrustumCullingSystem& GetFrustumCullingSystem() const { return frustumCullingSystem_; }
		void SetCullingCameraMode(CullingCameraMode mode) { cullingCameraMode_ = mode; }
		CullingCameraMode GetCullingCameraMode() const { return cullingCameraMode_; }
		void SetFrustumCullingEnabled(bool enabled) { frustumCullingSystem_.SetEnabled(enabled); }
		bool IsFrustumCullingEnabled() const { return frustumCullingSystem_.IsEnabled(); }
		int GetDrawnObjectCount() const { return frustumCullingSystem_.GetDrawnObjectCount(); }
		int GetCulledObjectCount() const { return frustumCullingSystem_.GetCulledObjectCount(); }
		int GetTotalObjectCount() const { return frustumCullingSystem_.GetTotalObjectCount(); }
		int GetCullingDisabledDrawnObjectCount() const { return frustumCullingSystem_.GetCullingDisabledDrawnObjectCount(); }
		int GetMissingBoundsDrawnObjectCount() const { return frustumCullingSystem_.GetMissingBoundsDrawnObjectCount(); }
		int GetDrawnMeshCount() const { return frustumCullingSystem_.GetDrawnMeshCount(); }
		int GetCulledMeshCount() const { return frustumCullingSystem_.GetCulledMeshCount(); }
		int GetTotalMeshCount() const { return frustumCullingSystem_.GetTotalMeshCount(); }
		int GetTotalStageObjectCount() const { return frustumCullingSystem_.GetTotalStageObjectCount(); }
		int GetDrawnStageObjectCount() const { return frustumCullingSystem_.GetDrawnStageObjectCount(); }
		int GetCulledStageObjectCount() const { return frustumCullingSystem_.GetCulledStageObjectCount(); }
		void SetBoundsDebugVisible(bool visible) { showBoundsDebug_ = visible; }
		bool IsBoundsDebugVisible() const { return showBoundsDebug_; }
		Matrix4x4 GetCullingViewProjectionMatrix() const;

	public: /// ---------- 拡張予定の関数 ---------- ///

		/*const PipelineBundle& GetForward() const;
		const PipelineBundle& GetDeferred() const;
		const PipelineBundle& GetShadow() const;
		const PipelineBundle& GetAlphaClipped() const;*/

	private:
		struct PointShadowPassGPU
		{
			Vector4 lightPositionAndFar{};
		};

		Object3DCommon() = default;
		~Object3DCommon() = default;
		Object3DCommon(const Object3DCommon&) = delete;
		Object3DCommon& operator=(const Object3DCommon&) = delete;

		void UpdatePointShadowPassData();

	private:
		DirectXCommon* dxCommon_ = nullptr;

		Object3DPipelineSet pipelineSet_{};
		InstancedObject3DPipelineSet instancedPipelineSet_{};
		ShadowCasterPipelineSet shadowCasterPipelineSet_{};
		ComPtr<ID3D12Resource> pointShadowPassResource_{};
		PointShadowPassGPU* pointShadowPassData_ = nullptr;

		FrustumCullingSystem frustumCullingSystem_{};
		CullingCameraMode cullingCameraMode_ = CullingCameraMode::MainCamera;
		bool showBoundsDebug_ = false;
	};
} // namespace Ken4lowEngine
