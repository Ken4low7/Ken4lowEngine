#define NOMINMAX
#include "Object3DCommon.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "DebugCamera.h"
#include "Engine/Graphics/Culling/CullingDiagnostics.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "ImGuiManager.h"
#include <imgui.h>
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
		CullingDiagnostics::GetInstance()->BeginMainPass(); // Shadowとは分け、Main 3D passだけのSurface統計を毎フレーム取り直す。
	}

	void Object3DCommon::EndObject3DPass()
	{
		CullingDiagnostics::GetInstance()->EndMainPass(); // Selection/Picking/OverlayのDrawをMain World統計へ混ぜない。
	}

	void Object3DCommon::DrawImGui()
	{
#ifdef USE_IMGUI
		if (ImGui::Begin("Culling Statistics"))
		{
			const auto* diagnostics = CullingDiagnostics::GetInstance();
			const auto& stats = diagnostics->GetSnapshot();
			const auto toUll = [](uint64_t value) { return static_cast<unsigned long long>(value); };

			if (ImGui::CollapsingHeader("Frustum", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const int totalObjects = GetTotalObjectCount();
				const int totalMeshes = GetTotalMeshCount();
				const float objectCullRate = totalObjects > 0 ? 100.0f * static_cast<float>(GetCulledObjectCount()) / static_cast<float>(totalObjects) : 0.0f;
				const float meshCullRate = totalMeshes > 0 ? 100.0f * static_cast<float>(GetCulledMeshCount()) / static_cast<float>(totalMeshes) : 0.0f;
				ImGui::Text("Frustum Culling: %s", IsFrustumCullingEnabled() ? "ON" : "OFF");
				ImGui::Text("Objects: submitted %d / drawn %d / culled %d (%.1f%%)", totalObjects, GetDrawnObjectCount(), GetCulledObjectCount(), objectCullRate);
				ImGui::Text("Meshes:  submitted %d / drawn %d / culled %d (%.1f%%)", totalMeshes, GetDrawnMeshCount(), GetCulledMeshCount(), meshCullRate);
				ImGui::Text("Missing Bounds Draws: %d", GetMissingBoundsDrawnObjectCount());
			}

			if (ImGui::CollapsingHeader("Surface / Rasterizer", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Tracked Indexed Draw Calls: %llu", toUll(stats.indexedDrawCalls));
				ImGui::Text("Submitted Surface Instances: %llu", toUll(stats.submittedSurfaceInstances));
				ImGui::Text("Submitted Triangles: %llu", toUll(stats.submittedTriangles));
				ImGui::Separator();
				ImGui::Text("Back   : draws %llu | triangles %llu", toUll(stats.drawCallsByCullMode[0]), toUll(stats.trianglesByCullMode[0]));
				ImGui::Text("Front  : draws %llu | triangles %llu", toUll(stats.drawCallsByCullMode[1]), toUll(stats.trianglesByCullMode[1]));
				ImGui::Text("TwoSide: draws %llu | triangles %llu", toUll(stats.drawCallsByCullMode[2]), toUll(stats.trianglesByCullMode[2]));
				ImGui::Text("One-Sided Triangles: %llu", toUll(diagnostics->GetOneSidedTriangleCount()));
				ImGui::Text("Estimated Back-Face Rejects*: %llu", toUll(diagnostics->GetEstimatedRasterizerRejectedTriangles()));
				ImGui::TextDisabled("* Diagnostic 50%% estimate only; it is not a GPU pipeline-statistics query.");
				ImGui::Separator();
				ImGui::Text("PSO Binds Back / Front / TwoSide: %llu / %llu / %llu",
					toUll(stats.pipelineBindsByCullMode[0]), toUll(stats.pipelineBindsByCullMode[1]), toUll(stats.pipelineBindsByCullMode[2]));
				ImGui::Text("PSO Binds Static / Alpha / Instanced / Animated: %llu / %llu / %llu / %llu",
					toUll(stats.pipelineBindsByPath[0]), toUll(stats.pipelineBindsByPath[1]),
					toUll(stats.pipelineBindsByPath[2]), toUll(stats.pipelineBindsByPath[3]));
			}

			if (ImGui::CollapsingHeader("Normal Cone / Visibility Meshlet", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Visibility Meshlet Instances: %llu", toUll(stats.visibilityMeshletInstances));
				ImGui::Text("Candidate Draw Calls: %llu", toUll(stats.normalConeCandidateDrawCalls));
				ImGui::Text("Candidate Meshlet Instances: %llu", toUll(stats.normalConeCandidateMeshletInstances));
				ImGui::Text("Candidate Triangles: %llu", toUll(stats.normalConeCandidateTriangles));
				ImGui::Separator();
				ImGui::Text("Meshlets Static / Alpha / Instanced / Animated: %llu / %llu / %llu / %llu",
					toUll(stats.visibilityMeshletInstancesByPath[0]), toUll(stats.visibilityMeshletInstancesByPath[1]),
					toUll(stats.visibilityMeshletInstancesByPath[2]), toUll(stats.visibilityMeshletInstancesByPath[3]));
				ImGui::Text("Animated Bind-Pose Candidates: draws %llu | meshlets %llu | triangles %llu",
					toUll(stats.normalConeCandidateDrawCallsByPath[3]),
					toUll(stats.normalConeCandidateMeshletInstancesByPath[3]),
					toUll(stats.normalConeCandidateTrianglesByPath[3]));
				ImGui::Text("Meshlet Budget: 64 vertices / 126 triangles");
				ImGui::Text("Runtime Normal Cone Culling: OFF");
				ImGui::TextDisabled("Animated values use bind-pose metadata only; skinned output is never rejected from these counters yet.");
			}
		}
		ImGui::End();
#endif // USE_IMGUI
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
		CullingDiagnostics::GetInstance()->SetActiveSurface(cullMode, CullingDiagnostics::SurfacePath::Static);
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
		CullingDiagnostics::GetInstance()->SetActiveSurface(cullMode, CullingDiagnostics::SurfacePath::Alpha);
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetAlpha(cullMode);

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(16, 17, 18); // 残像も通常Object3Dと同じLight・Shadow契約で描画する。
	}

	void Object3DCommon::SetAdditiveRenderSetting(MaterialCullMode cullMode)
	{
		CullingDiagnostics::GetInstance()->SetActiveSurface(cullMode, CullingDiagnostics::SurfacePath::Alpha);
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const PipelineBundle& pipeline = pipelineSet_.GetAdditive(cullMode);

		commandList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
		commandList->SetPipelineState(pipeline.pipelineState.Get());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		LightManager::GetInstance()->BindPunctualLights(5, 6);
		LightManager::GetInstance()->BindLightingSettings(11);
		LightManager::GetInstance()->BindExtendedShadowResources(16, 17, 18); // Additiveも通常Surfaceと同じLighting Root契約を維持する。
	}

	void Object3DCommon::SetInstancedRenderSetting(MaterialCullMode cullMode)
	{
		CullingDiagnostics::GetInstance()->SetActiveSurface(cullMode, CullingDiagnostics::SurfacePath::Instanced);
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

	void Object3DCommon::SetShadowMapRenderSetting(MaterialCullMode cullMode)
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const LightManager::ShadowCasterType casterType = LightManager::GetInstance()->GetActiveShadowCasterType();
		const bool isLocalLinearShadow = casterType == LightManager::ShadowCasterType::Point || casterType == LightManager::ShadowCasterType::Spot;
		const PipelineBundle& pipeline = isLocalLinearShadow
			? shadowCasterPipelineSet_.GetObjectPoint(cullMode)
			: shadowCasterPipelineSet_.GetObjectDepth(cullMode);

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

	void Object3DCommon::SetInstancedShadowMapRenderSetting(MaterialCullMode cullMode)
	{
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const LightManager::ShadowCasterType casterType = LightManager::GetInstance()->GetActiveShadowCasterType();
		const bool isLocalLinearShadow = casterType == LightManager::ShadowCasterType::Point || casterType == LightManager::ShadowCasterType::Spot;
		const PipelineBundle& pipeline = isLocalLinearShadow
			? shadowCasterPipelineSet_.GetInstancedPoint(cullMode)
			: shadowCasterPipelineSet_.GetInstancedDepth(cullMode);

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