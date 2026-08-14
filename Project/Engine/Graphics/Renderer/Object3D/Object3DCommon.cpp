#define NOMINMAX
#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "DebugCamera.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#endif // USE_IMGUI

namespace Ken4lowEngine
{
	Object3DCommon* Object3DCommon::GetInstance()
	{
		static Object3DCommon instance;
		return &instance;
	}

	void Object3DCommon::Initialize(DirectXCommon* dxCommon)
	{
		dxCommon_ = dxCommon;
		cullingCameraMode_ = CullingCameraMode::MainCamera;

		pipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);
		instancedPipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_D24_UNORM_S8_UINT);
		shadowCasterPipelineSet_.Initialize(dxCommon_->GetPipelineFactory(), dxCommon_->GetDXCCompilerManager());
		pointShadowPassData_.lightPositionAndFar = { 0.0f, 0.0f, 0.0f, 1.0f };

		LightManager::GetInstance()->Initialize(dxCommon_);
	}

	void Object3DCommon::Finalize()
	{
		LightManager::GetInstance()->Finalize();
		shadowCasterPipelineSet_.Finalize();
		pipelineSet_.Finalize();
		instancedPipelineSet_.Finalize();
		cullingCameraMode_ = CullingCameraMode::MainCamera;
		dxCommon_ = nullptr;
	}

	void Object3DCommon::BeginObject3DPass()
	{
		frustumCullingSystem_.ResetStatistics();
		frustumCullingSystem_.BuildFrustum(GetCullingViewProjectionMatrix());
	}

	void Object3DCommon::DrawImGui()
	{
		// Frustum Culling の確認 UI は ApplicationLayer の Controller に集約する。
	}

	Matrix4x4 Object3DCommon::GetCullingViewProjectionMatrix() const
	{
		auto* cameraManager = CameraManager::GetInstance();
		switch (cullingCameraMode_)
		{
		case CullingCameraMode::MainCamera:
			if (Camera* mainCamera = cameraManager->GetMainCamera())
			{
				return mainCamera->GetViewProjectionMatrix();
			}
			break;
		case CullingCameraMode::DebugCamera:
#ifdef _DEBUG
			if (DebugCamera* debugCamera = cameraManager->GetDebugCamera())
			{
				return debugCamera->GetViewProjectionMatrix();
			}
#endif
			break;
		case CullingCameraMode::ActiveCamera:
		default:
			return cameraManager->GetActiveViewProjectionMatrix();
		}

		return cameraManager->GetActiveViewProjectionMatrix();
	}

	void Object3DCommon::SetRenderSetting(MaterialCullMode cullMode)
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetDefault(cullMode);

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(16, 17, 18);
	}

	void Object3DCommon::SetAlphaRenderSetting(MaterialCullMode cullMode)
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetAlpha(cullMode);

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(16, 17, 18); // 残像も通常Object3Dと同じLight・Shadow契約で描画する。
	}

	void Object3DCommon::SetInstancedRenderSetting(MaterialCullMode cullMode)
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = instancedPipelineSet_.GetDefault(cullMode);

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(17, 18, 19);
	}

	bool Object3DCommon::ShouldDrawObject(const BoundingSphere& worldBounds, bool objectCullingEnabled, bool hasBounds, bool isStageObject)
	{
		const auto unit = isStageObject ? FrustumCullingSystem::CullingUnit::StageObject : FrustumCullingSystem::CullingUnit::Object;
		return frustumCullingSystem_.IsVisible(worldBounds, !objectCullingEnabled, hasBounds, unit);
	}

	bool Object3DCommon::ShouldDrawMesh(const BoundingSphere& worldBounds, bool objectCullingEnabled, bool hasBounds)
	{
		return frustumCullingSystem_.IsVisible(worldBounds, !objectCullingEnabled, hasBounds, FrustumCullingSystem::CullingUnit::Mesh);
	}

	void Object3DCommon::UpdatePointShadowPassData()
	{
		int32_t lightIndex = -1;
		LightManager::PunctualLightGPU light{};
		LightManager::ShadowCasterType casterType = LightManager::ShadowCasterType::None;
		if (!LightManager::GetInstance()->TryGetActiveShadowCasterLightInfo(lightIndex, light, casterType) ||
			(casterType != LightManager::ShadowCasterType::Point && casterType != LightManager::ShadowCasterType::Spot))
		{
			pointShadowPassData_.lightPositionAndFar = { 0.0f, 0.0f, 0.0f, 1.0f };
			return;
		}

		const float farZ = std::max({ std::fabs(light.radius), std::fabs(light.distance), 1.0f });
		pointShadowPassData_.lightPositionAndFar = { light.position.x, light.position.y, light.position.z, farZ };
	}

	void Object3DCommon::SetShadowMapRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const LightManager::ShadowCasterType casterType = LightManager::GetInstance()->GetActiveShadowCasterType();
		const bool isLocalLinearShadow = casterType == LightManager::ShadowCasterType::Point || casterType == LightManager::ShadowCasterType::Spot;
		const PipelineBundle& pipeline = isLocalLinearShadow
			? shadowCasterPipelineSet_.GetObjectPoint()
			: shadowCasterPipelineSet_.GetObjectDepth();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (isLocalLinearShadow)
		{
			UpdatePointShadowPassData();
			const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(pointShadowPassData_);
			if (allocation.IsValid())
			{
				commandList->SetGraphicsRootConstantBufferView(1, allocation.gpuAddress); // Shadow Passごとに固有CBを確保し、後続Lightによる上書きを防ぐ。
			}
		}
	}

	void Object3DCommon::SetInstancedShadowMapRenderSetting()
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const LightManager::ShadowCasterType casterType = LightManager::GetInstance()->GetActiveShadowCasterType();
		const bool isLocalLinearShadow = casterType == LightManager::ShadowCasterType::Point || casterType == LightManager::ShadowCasterType::Spot;
		const PipelineBundle& pipeline = isLocalLinearShadow
			? shadowCasterPipelineSet_.GetInstancedPoint()
			: shadowCasterPipelineSet_.GetInstancedDepth();

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		if (isLocalLinearShadow)
		{
			UpdatePointShadowPassData();
			const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(pointShadowPassData_);
			if (allocation.IsValid())
			{
				commandList->SetGraphicsRootConstantBufferView(2, allocation.gpuAddress); // Instancing ShadowもPass固有CBでb1を束縛する。
			}
		}
	}
} // namespace Ken4lowEngine
