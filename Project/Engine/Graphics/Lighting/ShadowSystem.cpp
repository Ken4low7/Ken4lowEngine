#define NOMINMAX
#include "ShadowSystem.h"

#include "ShadowMapArrayRenderTarget.h"
#include "DirectXCommon.h"
#include "CameraManager.h"
#include "ResourceManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace Ken4lowEngine
{
	ShadowSystem::ShadowSystem() = default;
	ShadowSystem::~ShadowSystem() = default;

	namespace
	{
		constexpr uint32_t kDirectionalShadowTechnique = 1;
		constexpr uint32_t kSpotLinearShadowTechnique = 2;
		constexpr uint32_t kPointShadowTechnique = 3;
		constexpr uint32_t kCsmShadowTechnique = 4;

		float ClampPositive(float value, float fallback)
		{
			return std::isfinite(value) && value > 0.0f ? value : fallback;
		}
	}

	void ShadowSystem::Initialize(DirectXCommon* dxCommon, uint32_t shadowMapSize)
	{
		dxCommon_ = dxCommon;
		const uint32_t safeSize = std::max(shadowMapSize, 1u);
		desiredMapSize_ = safeSize;
		constexpr uint32_t kInactivePlaceholderSize = 64; // 未使用方式の巨大配列を起動時に確保しない。
		csmRenderTarget_ = std::make_unique<ShadowMapArrayRenderTarget>();
		csmRenderTarget_->Initialize(dxCommon_, kInactivePlaceholderSize, kInactivePlaceholderSize, kCascadeCount, ShadowArrayViewType::Texture2DArray, L"CascadedShadowMapArray");
		pointRenderTarget_ = std::make_unique<ShadowMapArrayRenderTarget>();
		pointRenderTarget_->Initialize(dxCommon_, kInactivePlaceholderSize, kInactivePlaceholderSize, kPointFaceCount, ShadowArrayViewType::TextureCube, L"PointLightCubeShadowMap");

		extendedShadowResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(ExtendedShadowParameterGPU));
		extendedShadowResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuData_));
		*gpuData_ = ExtendedShadowParameterGPU{};
		for (Matrix4x4& matrix : gpuData_->cascadeLightViewProjection) { matrix = Matrix4x4::MakeIdentity(); }
		extendedShadowResource_->SetName(L"ExtendedShadowParameterConstantBuffer");
	}

	void ShadowSystem::Finalize()
	{
		if (csmRenderTarget_) { csmRenderTarget_->Finalize(); }
		if (pointRenderTarget_) { pointRenderTarget_->Finalize(); }
		csmRenderTarget_.reset();
		pointRenderTarget_.reset();
		if (extendedShadowResource_) { extendedShadowResource_->Unmap(0, nullptr); }
		gpuData_ = nullptr;
		extendedShadowResource_.Reset();
		dxCommon_ = nullptr;
		desiredMapSize_ = 2048;
	}

	void ShadowSystem::Resize(uint32_t shadowMapSize)
	{
		const uint32_t safeSize = std::max(shadowMapSize, 1u);
		desiredMapSize_ = safeSize; // 実Resourceはその方式が次に選ばれた時だけ再生成する。
	}

	void ShadowSystem::Execute(LightManager& lightManager, const std::function<void()>& drawShadowObjects)
	{
		ResetGpuData(lightManager);
		int32_t lightIndex = -1;
		LightManager::PunctualLightGPU light{};
		LightManager::ShadowCasterType casterType = LightManager::ShadowCasterType::None;
		if (!lightManager.enableShadow_ || !lightManager.TryGetActiveShadowCasterLightInfo(lightIndex, light, casterType))
		{
			// 既存と同じくShadow Map自体は毎Frame更新し、Shadow OFF時の描画順を変えない。
			ExecuteLegacyPass(lightManager, drawShadowObjects);
			csmRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
			pointRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
			return;
		}

		const uint32_t gpuLightIndex = ResolveGpuLightIndex(lightManager, lightIndex);
		gpuData_->shadowCasterLightIndex = gpuLightIndex; // 通常描画側は選択中ライトだけへShadowを適用する。

		if (casterType == LightManager::ShadowCasterType::Point)
		{
			ExecutePointPass(lightManager, light, gpuLightIndex, drawShadowObjects);
			dxCommon_->EndShadowMapPass(); // 未使用のLegacy t4も通常描画前にSRV状態へ揃える。
			csmRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
		}
		else if (casterType == LightManager::ShadowCasterType::Directional && lightManager.enableCsm_)
		{
			ExecuteCsmPass(lightManager, light, gpuLightIndex, drawShadowObjects);
			dxCommon_->EndShadowMapPass();
			pointRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
		}
		else
		{
			if (casterType == LightManager::ShadowCasterType::Spot)
			{
				const float farZ = std::max(ClampPositive(light.distance, 1.0f), 1.0f);
				const float nearZ = std::clamp(lightManager.spotShadowNearZ_, 0.01f, farZ * 0.5f);
				gpuData_->shadowTechnique = kSpotLinearShadowTechnique;
				gpuData_->pointLightPositionAndFar = { light.position.x, light.position.y, light.position.z, farZ };
				gpuData_->cameraPositionAndPointNear.w = nearZ;
			}
			else if (casterType == LightManager::ShadowCasterType::Directional)
			{
				gpuData_->shadowTechnique = kDirectionalShadowTechnique;
			}

			ExecuteLegacyPass(lightManager, drawShadowObjects);
			csmRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
			pointRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
		}
	}

	void ShadowSystem::Bind(uint32_t extendedShadowCbvRootIndex, uint32_t csmSrvRootIndex, uint32_t pointSrvRootIndex) const
	{
		if (!dxCommon_ || !extendedShadowResource_ || !csmRenderTarget_ || !pointRenderTarget_) { return; }
		auto* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->SetGraphicsRootConstantBufferView(extendedShadowCbvRootIndex, extendedShadowResource_->GetGPUVirtualAddress());
		commandList->SetGraphicsRootDescriptorTable(csmSrvRootIndex, csmRenderTarget_->GetSrvHandleGPU());
		commandList->SetGraphicsRootDescriptorTable(pointSrvRootIndex, pointRenderTarget_->GetSrvHandleGPU());
	}

	void ShadowSystem::ExecuteLegacyPass(LightManager& lightManager, const std::function<void()>& drawShadowObjects)
	{
		activePassLightViewProjection_ = lightManager.BuildShadowLightViewProjection(CameraManager::GetInstance()->GetActiveCameraPosition());
		gpuData_->cascadeLightViewProjection[0] = activePassLightViewProjection_; // Receiverは各Objectの更新状態ではなくFrame共通行列を参照する。
		dxCommon_->BeginShadowMapPass();
		if (drawShadowObjects) { drawShadowObjects(); }
		dxCommon_->EndShadowMapPass();
	}

	void ShadowSystem::ExecutePointPass(LightManager& lightManager, const LightManager::PunctualLightGPU& light, uint32_t gpuLightIndex, const std::function<void()>& drawShadowObjects)
	{
		pointRenderTarget_->Resize(desiredMapSize_, desiredMapSize_);
		const float farZ = std::max({ ClampPositive(light.radius, 1.0f), ClampPositive(light.distance, 1.0f), 1.0f });
		const float nearZ = std::clamp(lightManager.pointShadowNearZ_, 0.01f, farZ * 0.5f);
		const Matrix4x4 projection = Matrix4x4::MakePerspectiveFovMatrix(std::numbers::pi_v<float> * 0.5f, 1.0f, nearZ, farZ);
		const std::array<Vector3, kPointFaceCount> directions = {
			Vector3{ 1.0f, 0.0f, 0.0f }, Vector3{ -1.0f, 0.0f, 0.0f },
			Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, -1.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, 1.0f }, Vector3{ 0.0f, 0.0f, -1.0f }
		};
		const std::array<Vector3, kPointFaceCount> upVectors = {
			Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, -1.0f }, Vector3{ 0.0f, 0.0f, 1.0f },
			Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f }
		};

		gpuData_->shadowTechnique = kPointShadowTechnique;
		gpuData_->shadowCasterLightIndex = gpuLightIndex;
		gpuData_->pointLightPositionAndFar = { light.position.x, light.position.y, light.position.z, farZ };
		gpuData_->cameraPositionAndPointNear.w = nearZ;
		for (uint32_t face = 0; face < kPointFaceCount; ++face)
		{
			const Matrix4x4 view = Matrix4x4::MakeLookAtMatrix(light.position, light.position + directions[face], upVectors[face]);
			activePassLightViewProjection_ = Matrix4x4::Multiply(view, projection);
			pointRenderTarget_->BeginSlice(dxCommon_->GetCommandManager()->GetCommandList(), face);
			if (drawShadowObjects) { drawShadowObjects(); }
		}
		pointRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
	}

	void ShadowSystem::ExecuteCsmPass(LightManager& lightManager, const LightManager::PunctualLightGPU& light, uint32_t gpuLightIndex, const std::function<void()>& drawShadowObjects)
	{
		csmRenderTarget_->Resize(desiredMapSize_, desiredMapSize_);
		auto* cameraManager = CameraManager::GetInstance();
		const float cameraNear = std::max(cameraManager->GetActiveNearClip(), 0.01f);
		const float cameraFar = std::max(cameraManager->GetActiveFarClip(), cameraNear + 1.0f);
		const float shadowFar = std::clamp(lightManager.csmMaxDistance_, cameraNear + 1.0f, cameraFar);
		const float lambda = std::clamp(lightManager.csmSplitLambda_, 0.0f, 1.0f);
		std::array<float, kCascadeCount> splits{};
		for (uint32_t i = 0; i < kCascadeCount; ++i)
		{
			const float p = static_cast<float>(i + 1) / static_cast<float>(kCascadeCount);
			const float logarithmic = cameraNear * std::pow(shadowFar / cameraNear, p);
			const float uniform = cameraNear + (shadowFar - cameraNear) * p;
			splits[i] = std::lerp(uniform, logarithmic, lambda);
		}

		const Vector3 cameraPosition = cameraManager->GetActiveCameraPosition();
		const Vector3 forward = Vector3::NormalizeSafe(cameraManager->GetActiveCameraForward(), { 0.0f, 0.0f, 1.0f });
		const Vector3 worldUp = std::fabs(Vector3::Dot(forward, { 0.0f, 1.0f, 0.0f })) > 0.98f ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
		const Vector3 right = Vector3::NormalizeSafe(Vector3::Cross(worldUp, forward), { 1.0f, 0.0f, 0.0f });
		const Vector3 up = Vector3::NormalizeSafe(Vector3::Cross(forward, right), { 0.0f, 1.0f, 0.0f });
		const float tanHalfFov = std::tan(cameraManager->GetActiveFovY() * 0.5f);
		const float aspect = std::max(cameraManager->GetActiveAspectRatio(), 0.01f);
		const Vector3 lightDirection = Vector3::NormalizeSafe(light.direction, { 0.3f, -1.0f, 0.2f });

		gpuData_->shadowTechnique = kCsmShadowTechnique;
		gpuData_->shadowCasterLightIndex = gpuLightIndex;
		gpuData_->pointLightPositionAndFar = { forward.x, forward.y, forward.z, shadowFar }; // CSM時はxyzをCamera ForwardとしてCascade選択へ使う。

		float sliceNear = cameraNear;
		for (uint32_t cascade = 0; cascade < kCascadeCount; ++cascade)
		{
			const float sliceFar = splits[cascade];
			const float nearHeight = tanHalfFov * sliceNear;
			const float nearWidth = nearHeight * aspect;
			const float farHeight = tanHalfFov * sliceFar;
			const float farWidth = farHeight * aspect;
			const Vector3 nearCenter = cameraPosition + forward * sliceNear;
			const Vector3 farCenter = cameraPosition + forward * sliceFar;
			std::array<Vector3, 8> corners = {
				nearCenter + up * nearHeight - right * nearWidth,
				nearCenter + up * nearHeight + right * nearWidth,
				nearCenter - up * nearHeight - right * nearWidth,
				nearCenter - up * nearHeight + right * nearWidth,
				farCenter + up * farHeight - right * farWidth,
				farCenter + up * farHeight + right * farWidth,
				farCenter - up * farHeight - right * farWidth,
				farCenter - up * farHeight + right * farWidth
			};
			Vector3 center{};
			for (const Vector3& corner : corners) { center += corner; }
			center /= static_cast<float>(corners.size());
			float radius = 0.0f;
			for (const Vector3& corner : corners) { radius = std::max(radius, Vector3::Length(corner - center)); }
			radius = std::ceil(std::max(radius, 1.0f) * 16.0f) / 16.0f;
			const float lightDistance = radius * 2.0f + 10.0f;
			const float lightFar = lightDistance + radius * 2.0f + 20.0f;
			gpuData_->cascadeLightViewProjection[cascade] = BuildStableDirectionalMatrix(
				lightDirection, center, radius, radius, lightDistance, 0.1f, lightFar, csmRenderTarget_->GetWidth());
			activePassLightViewProjection_ = gpuData_->cascadeLightViewProjection[cascade];
			csmRenderTarget_->BeginSlice(dxCommon_->GetCommandManager()->GetCommandList(), cascade);
			if (drawShadowObjects) { drawShadowObjects(); }
			sliceNear = sliceFar;
		}
		csmRenderTarget_->End(dxCommon_->GetCommandManager()->GetCommandList());
		gpuData_->cascadeSplits = { splits[0], splits[1], splits[2], splits[3] };
		gpuData_->cascadeCount = kCascadeCount;
	}

	uint32_t ShadowSystem::ResolveGpuLightIndex(const LightManager& lightManager, int32_t legacyLightIndex) const
	{
		if (legacyLightIndex < 0) { return UINT32_MAX; }
		uint32_t gpuIndex = 0;
		const int32_t globalCount = static_cast<int32_t>(lightManager.punctualLights_.size());
		for (int32_t i = 0; i < std::min(legacyLightIndex, globalCount); ++i)
		{
			const auto& light = lightManager.punctualLights_[i];
			if (light.lightType != 0 && light.enabled != 0u) { ++gpuIndex; }
		}
		if (legacyLightIndex > globalCount)
		{
			for (int32_t i = 0; i < legacyLightIndex - globalCount && i < static_cast<int32_t>(lightManager.lightComponentLights_.size()); ++i)
			{
				const auto& light = lightManager.lightComponentLights_[i];
				if (light.lightType != 0 && light.enabled != 0u) { ++gpuIndex; }
			}
		}
		return gpuIndex;
	}

	Matrix4x4 ShadowSystem::BuildStableDirectionalMatrix(const Vector3& direction, const Vector3& focus, float halfWidth, float halfHeight, float distance, float nearZ, float farZ, uint32_t mapSize) const
	{
		const Vector3 lightDirection = Vector3::NormalizeSafe(direction, { 0.3f, -1.0f, 0.2f });
		const Vector3 referenceUp = std::fabs(Vector3::Dot(lightDirection, { 0.0f, 1.0f, 0.0f })) > 0.98f ? Vector3{ 1.0f, 0.0f, 0.0f } : Vector3{ 0.0f, 1.0f, 0.0f };
		const Vector3 right = Vector3::NormalizeSafe(Vector3::Cross(referenceUp, lightDirection), { 1.0f, 0.0f, 0.0f });
		const Vector3 up = Vector3::NormalizeSafe(Vector3::Cross(lightDirection, right), { 0.0f, 1.0f, 0.0f });
		const float texelX = (halfWidth * 2.0f) / static_cast<float>(std::max(mapSize, 1u));
		const float texelY = (halfHeight * 2.0f) / static_cast<float>(std::max(mapSize, 1u));
		const float projectedX = Vector3::Dot(focus, right);
		const float projectedY = Vector3::Dot(focus, up);
		Vector3 stableFocus = focus;
		stableFocus += right * (std::round(projectedX / texelX) * texelX - projectedX);
		stableFocus += up * (std::round(projectedY / texelY) * texelY - projectedY);
		return Matrix4x4::MakeLightViewProjection(lightDirection, stableFocus, distance, halfWidth, halfHeight, nearZ, farZ);
	}

	void ShadowSystem::ResetGpuData(const LightManager& lightManager)
	{
		if (!gpuData_) { return; }
		for (Matrix4x4& matrix : gpuData_->cascadeLightViewProjection) { matrix = Matrix4x4::MakeIdentity(); }
		gpuData_->cascadeSplits = {};
		gpuData_->pointLightPositionAndFar = {};
		const Vector3 cameraPosition = CameraManager::GetInstance()->GetActiveCameraPosition();
		gpuData_->cameraPositionAndPointNear = { cameraPosition.x, cameraPosition.y, cameraPosition.z, lightManager.pointShadowNearZ_ };
		gpuData_->shadowTechnique = 0;
		gpuData_->cascadeCount = 0;
		gpuData_->shadowCasterLightIndex = UINT32_MAX;
		gpuData_->shadowBias = lightManager.shadowBias_;
		gpuData_->normalBias = lightManager.normalBias_;
		gpuData_->shadowStrength = lightManager.shadowStrength_;
	}
}