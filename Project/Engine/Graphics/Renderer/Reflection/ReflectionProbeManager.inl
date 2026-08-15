#include "CameraManager.h"
#include "DirectXCommon.h"
#include "DSVManager.h"
#include "RTVManager.h"
#include "SRVManager.h"
#include "Engine/Graphics/Renderer/Environment/EnvironmentMapManager.h"
#include "Engine/Graphics/Renderer/Object3D/Object3DCommon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>

namespace Ken4lowEngine
{
	namespace ReflectionProbeDetail
	{
		constexpr uint32_t kCubeFaceCount = 6;
		constexpr float kMinimumProbeRadius = 0.1f;
		constexpr float kMinimumNearClip = 0.01f;
		constexpr float kMinimumFarGap = 0.1f;

		inline bool NearlyEqual(float lhs, float rhs)
		{
			return std::fabs(lhs - rhs) <= 0.0001f;
		}

		inline bool SamePosition(const Vector3& lhs, const Vector3& rhs)
		{
			return NearlyEqual(lhs.x, rhs.x) && NearlyEqual(lhs.y, rhs.y) && NearlyEqual(lhs.z, rhs.z);
		}

		inline uint32_t SanitizeResolution(uint32_t resolution)
		{
			constexpr std::array<uint32_t, 4> supported{ 64, 128, 256, 512 };
			uint32_t best = supported.front();
			uint32_t bestDistance = (std::numeric_limits<uint32_t>::max)();
			for (uint32_t candidate : supported)
			{
				const uint32_t distance = candidate > resolution ? candidate - resolution : resolution - candidate;
				if (distance < bestDistance)
				{
					best = candidate;
					bestDistance = distance;
				}
			}
			return best;
		}
	}

	inline ReflectionProbeManager* ReflectionProbeManager::GetInstance()
	{
		static ReflectionProbeManager instance;
		return &instance;
	}

	inline void ReflectionProbeManager::Initialize(DirectXCommon* dxCommon)
	{
		Finalize();
		dxCommon_ = dxCommon;
		captureSerial_ = 0;
		isCapturing_ = false;
	}

	inline void ReflectionProbeManager::Finalize()
	{
		CameraManager* cameraManager = CameraManager::GetInstance();
		while (cameraManager->HasRenderViewOverride())
		{
			cameraManager->PopRenderViewOverride(); // Capture中断時も一時Viewを残さず通常Cameraへ戻す。
		}

		for (ProbeRuntime& probe : probes_)
		{
			if (probe.target) ReleaseTarget(*probe.target);
		}
		for (std::unique_ptr<ProbeTarget>& target : retiredTargets_)
		{
			if (target) ReleaseTarget(*target);
		}
		probes_.clear();
		retiredTargets_.clear();
		isCapturing_ = false;
		captureSerial_ = 0;
		dxCommon_ = nullptr;
	}

	inline ReflectionProbeDesc ReflectionProbeManager::SanitizeDesc(const ReflectionProbeDesc& desc) const
	{
		ReflectionProbeDesc sanitized = desc;
		sanitized.influenceRadius = (std::max)(desc.influenceRadius, ReflectionProbeDetail::kMinimumProbeRadius);
		sanitized.nearClip = (std::max)(desc.nearClip, ReflectionProbeDetail::kMinimumNearClip);
		sanitized.farClip = (std::max)(desc.farClip, sanitized.nearClip + ReflectionProbeDetail::kMinimumFarGap);
		sanitized.resolution = ReflectionProbeDetail::SanitizeResolution(desc.resolution);
		return sanitized;
	}

	inline void ReflectionProbeManager::RegisterOrUpdateProbe(const void* owner, const ReflectionProbeDesc& desc, bool forceDirty)
	{
		if (!owner) return;
		const ReflectionProbeDesc sanitized = SanitizeDesc(desc);
		ProbeRuntime* probe = FindProbe(owner);
		if (!probe)
		{
			ProbeRuntime runtime{};
			runtime.owner = owner;
			runtime.desc = sanitized;
			runtime.dirty = true; // 新規Probeは最初の描画前に一度だけSceneをCaptureする。
			probes_.push_back(std::move(runtime));
			return;
		}

		const bool captureInputChanged =
			!ReflectionProbeDetail::SamePosition(probe->desc.position, sanitized.position) ||
			!ReflectionProbeDetail::NearlyEqual(probe->desc.nearClip, sanitized.nearClip) ||
			!ReflectionProbeDetail::NearlyEqual(probe->desc.farClip, sanitized.farClip) ||
			probe->desc.resolution != sanitized.resolution ||
			probe->desc.enabled != sanitized.enabled;
		probe->desc = sanitized;
		probe->dirty = probe->dirty || captureInputChanged || forceDirty;
	}

	inline void ReflectionProbeManager::UnregisterProbe(const void* owner)
	{
		ProbeRuntime* probe = FindProbe(owner);
		if (!probe) return;
		probe->owner = nullptr;
		probe->desc.enabled = false;
		probe->dirty = false; // ResourceはFrame中のGPU参照を避けるためManager終了まで保持する。
	}

	inline void ReflectionProbeManager::RequestCapture(const void* owner)
	{
		if (ProbeRuntime* probe = FindProbe(owner))
		{
			probe->dirty = true;
		}
	}

	inline ReflectionProbeManager::ProbeRuntime* ReflectionProbeManager::FindProbe(const void* owner)
	{
		if (!owner) return nullptr;
		const auto found = std::find_if(probes_.begin(), probes_.end(),
			[owner](const ProbeRuntime& probe) { return probe.owner == owner; });
		return found != probes_.end() ? &*found : nullptr;
	}

	inline const ReflectionProbeManager::ProbeRuntime* ReflectionProbeManager::FindProbe(const void* owner) const
	{
		if (!owner) return nullptr;
		const auto found = std::find_if(probes_.begin(), probes_.end(),
			[owner](const ProbeRuntime& probe) { return probe.owner == owner; });
		return found != probes_.end() ? &*found : nullptr;
	}

	inline ReflectionProbeManager::ProbeRuntime* ReflectionProbeManager::FindCaptureCandidate()
	{
		for (ProbeRuntime& probe : probes_)
		{
			if (!probe.owner || !probe.desc.enabled) continue;
			if (probe.desc.updateMode == ReflectionProbeUpdateMode::EveryFrame)
			{
				probe.dirty = true;
			}
			if (probe.dirty || !probe.captured) return &probe;
		}
		return nullptr;
	}

	inline bool ReflectionProbeManager::CapturePending(const std::function<void()>& drawStaticScene)
	{
		if (!dxCommon_ || isCapturing_ || !drawStaticScene) return false;
		ProbeRuntime* probe = FindCaptureCandidate();
		if (!probe) return false;
		return CaptureProbe(*probe, drawStaticScene); // 1フレーム1Probeに制限し、6面Captureのスパイクを連続発生させない。
	}

	inline bool ReflectionProbeManager::EnsureTarget(ProbeRuntime& probe)
	{
		if (probe.target && probe.target->resolution == probe.desc.resolution) return true;
		if (probe.target)
		{
			retiredTargets_.push_back(std::move(probe.target)); // 古いDescriptorをGPU参照中に再利用しない。
			probe.captured = false;
		}
		probe.target = CreateTarget(probe.desc.resolution);
		return probe.target != nullptr;
	}

	inline std::unique_ptr<ReflectionProbeManager::ProbeTarget> ReflectionProbeManager::CreateTarget(uint32_t resolution)
	{
		if (!dxCommon_ || !dxCommon_->GetDevice()) return nullptr;
		auto target = std::make_unique<ProbeTarget>();
		target->resolution = ReflectionProbeDetail::SanitizeResolution(resolution);

		D3D12_RESOURCE_DESC colorDesc{};
		colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		colorDesc.Width = target->resolution;
		colorDesc.Height = target->resolution;
		colorDesc.DepthOrArraySize = static_cast<UINT16>(ReflectionProbeDetail::kCubeFaceCount);
		colorDesc.MipLevels = 1;
		colorDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		colorDesc.SampleDesc.Count = 1;
		colorDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		colorDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		D3D12_CLEAR_VALUE colorClear{};
		colorClear.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		colorClear.Color[0] = 0.0f;
		colorClear.Color[1] = 0.0f;
		colorClear.Color[2] = 0.0f;
		colorClear.Color[3] = 1.0f;
		CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
		const HRESULT colorResult = dxCommon_->GetDevice()->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&colorDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			&colorClear,
			IID_PPV_ARGS(&target->color));
		if (FAILED(colorResult)) return nullptr;
		target->color->SetName(L"ReflectionProbeCube");
		target->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		RTVManager* rtvManager = RTVManager::GetInstance();
		for (uint32_t face = 0; face < ReflectionProbeDetail::kCubeFaceCount; ++face)
		{
			target->rtvIndices[face] = rtvManager->Allocate();
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = 0;
			rtvDesc.Texture2DArray.FirstArraySlice = face;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.PlaneSlice = 0;
			dxCommon_->GetDevice()->CreateRenderTargetView(
				target->color.Get(), &rtvDesc, rtvManager->GetCPUDescriptorHandle(target->rtvIndices[face]));
		}

		D3D12_CLEAR_VALUE depthClear{};
		target->depth = DSVManager::GetInstance()->CreateDepthStencilBuffer(
			target->resolution, target->resolution, DXGI_FORMAT_D24_UNORM_S8_UINT, depthClear);
		if (!target->depth)
		{
			ReleaseTarget(*target);
			return nullptr;
		}
		target->depth->SetName(L"ReflectionProbeDepth");
		target->dsvIndex = DSVManager::GetInstance()->Allocate();
		DSVManager::GetInstance()->CreateDSVForDepthBuffer(target->dsvIndex, target->depth.Get());

		target->srvIndex = SRVManager::GetInstance()->Allocate();
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MostDetailedMip = 0;
		srvDesc.TextureCube.MipLevels = 1;
		srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
		dxCommon_->GetDevice()->CreateShaderResourceView(
			target->color.Get(), &srvDesc, SRVManager::GetInstance()->GetCPUDescriptorHandle(target->srvIndex));

		target->viewport = D3D12_VIEWPORT(
			0.0f, 0.0f,
			static_cast<float>(target->resolution), static_cast<float>(target->resolution),
			0.0f, 1.0f);
		target->scissor = { 0, 0, static_cast<LONG>(target->resolution), static_cast<LONG>(target->resolution) };
		return target;
	}

	inline void ReflectionProbeManager::TransitionTarget(ProbeTarget& target, D3D12_RESOURCE_STATES nextState)
	{
		if (!target.color || target.state == nextState) return;
		dxCommon_->ResourceTransition(target.color.Get(), target.state, nextState);
		target.state = nextState;
	}

	inline void ReflectionProbeManager::BeginFace(ProbeTarget& target, uint32_t faceIndex)
	{
		if (faceIndex >= ReflectionProbeDetail::kCubeFaceCount || !target.color || !target.depth) return;
		TransitionTarget(target, D3D12_RESOURCE_STATE_RENDER_TARGET);

		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->RSSetViewports(1, &target.viewport);
		commandList->RSSetScissorRects(1, &target.scissor);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtv = RTVManager::GetInstance()->GetCPUDescriptorHandle(target.rtvIndices[faceIndex]);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv = DSVManager::GetInstance()->GetCPUDescriptorHandle(target.dsvIndex);
		commandList->OMSetRenderTargets(1, &rtv, false, &dsv);
		constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	}

	inline void ReflectionProbeManager::EndCapture(ProbeTarget& target)
	{
		TransitionTarget(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	inline void ReflectionProbeManager::ReleaseTarget(ProbeTarget& target)
	{
		RTVManager* rtvManager = RTVManager::GetInstance();
		for (uint32_t& rtvIndex : target.rtvIndices)
		{
			if (rtvIndex != UINT32_MAX)
			{
				rtvManager->Free(rtvIndex);
				rtvIndex = UINT32_MAX;
			}
		}
		if (target.dsvIndex != UINT32_MAX)
		{
			DSVManager::GetInstance()->Free(target.dsvIndex);
			target.dsvIndex = UINT32_MAX;
		}
		if (target.srvIndex != UINT32_MAX)
		{
			SRVManager::GetInstance()->Free(target.srvIndex);
			target.srvIndex = UINT32_MAX;
		}
		target.color.Reset();
		target.depth.Reset();
	}

	inline bool ReflectionProbeManager::CaptureProbe(ProbeRuntime& probe, const std::function<void()>& drawStaticScene)
	{
		if (!EnsureTarget(probe) || !probe.target) return false;

		const std::array<Vector3, ReflectionProbeDetail::kCubeFaceCount> directions = {
			Vector3{ 1.0f, 0.0f, 0.0f }, Vector3{ -1.0f, 0.0f, 0.0f },
			Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, -1.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, 1.0f }, Vector3{ 0.0f, 0.0f, -1.0f }
		};
		const std::array<Vector3, ReflectionProbeDetail::kCubeFaceCount> upVectors = {
			Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f },
			Vector3{ 0.0f, 0.0f, -1.0f }, Vector3{ 0.0f, 0.0f, 1.0f },
			Vector3{ 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f }
		};
		const float fovY = std::numbers::pi_v<float> * 0.5f;
		const Matrix4x4 projection = Matrix4x4::MakePerspectiveFovMatrix(
			fovY, 1.0f, probe.desc.nearClip, probe.desc.farClip);

		CameraManager* cameraManager = CameraManager::GetInstance();
		Object3DCommon* objectCommon = Object3DCommon::GetInstance();
		const Object3DCommon::CullingCameraMode previousCullingMode = objectCommon->GetCullingCameraMode();
		objectCommon->SetCullingCameraMode(Object3DCommon::CullingCameraMode::ActiveCamera);
		isCapturing_ = true; // Capture中は局所Probeの自己参照を禁止し、明示Global Environmentだけを許可する。

		bool succeeded = true;
		for (uint32_t face = 0; face < ReflectionProbeDetail::kCubeFaceCount; ++face)
		{
			const Matrix4x4 view = Matrix4x4::MakeLookAtMatrix(
				probe.desc.position, probe.desc.position + directions[face], upVectors[face]);
			CameraManager::RenderViewOverride renderView{};
			renderView.view = view;
			renderView.projection = projection;
			renderView.viewProjection = Matrix4x4::Multiply(view, projection);
			renderView.position = probe.desc.position;
			renderView.forward = directions[face];
			renderView.nearClip = probe.desc.nearClip;
			renderView.farClip = probe.desc.farClip;
			renderView.fovY = fovY;
			renderView.aspectRatio = 1.0f;

			cameraManager->PushRenderViewOverride(renderView);
			BeginFace(*probe.target, face);
			SRVManager::GetInstance()->PreDraw();
			objectCommon->BeginObject3DPass();

			try
			{
				drawStaticScene(); // v1は実際のScene GeometryだけをCaptureし、内部専用SkyBoxを勝手に合成しない。
			}
			catch (...)
			{
				succeeded = false;
			}
			objectCommon->EndObject3DPass();
			cameraManager->PopRenderViewOverride();
			if (!succeeded) break;
		}

		EndCapture(*probe.target);
		while (cameraManager->HasRenderViewOverride())
		{
			cameraManager->PopRenderViewOverride();
		}
		objectCommon->SetCullingCameraMode(previousCullingMode);
		isCapturing_ = false;

		if (!succeeded) return false;
		probe.captured = true;
		probe.dirty = false;
		probe.captureRevision = ++captureSerial_;
		return true;
	}

	inline D3D12_GPU_DESCRIPTOR_HANDLE ReflectionProbeManager::ResolveReflectionHandle(const Vector3& worldPosition) const
	{
		EnvironmentMapManager* environmentManager = EnvironmentMapManager::GetInstance();
		if (isCapturing_)
		{
			return environmentManager->HasGlobalReflectionSource()
				? environmentManager->GetGlobalEnvironmentMapHandle()
				: D3D12_GPU_DESCRIPTOR_HANDLE{};
		}

		const ProbeRuntime* nearest = nullptr;
		float nearestDistanceSquared = (std::numeric_limits<float>::max)();
		for (const ProbeRuntime& probe : probes_)
		{
			if (!probe.owner || !probe.desc.enabled || !probe.captured || !probe.target || probe.target->srvIndex == UINT32_MAX) continue;
			const Vector3 delta = worldPosition - probe.desc.position;
			const float distanceSquared = Vector3::Dot(delta, delta);
			const float radiusSquared = probe.desc.influenceRadius * probe.desc.influenceRadius;
			if (distanceSquared > radiusSquared || distanceSquared >= nearestDistanceSquared) continue;
			nearest = &probe;
			nearestDistanceSquared = distanceSquared;
		}

		if (nearest)
		{
			return SRVManager::GetInstance()->GetGPUDescriptorHandle(nearest->target->srvIndex);
		}
		if (environmentManager->HasGlobalReflectionSource())
		{
			return environmentManager->GetGlobalEnvironmentMapHandle();
		}
		return {}; // Probeも実際のEnvironmentも無いSceneでは、fallback Cubemapを反射源として見せない。
	}

	inline ReflectionProbeDiagnostics ReflectionProbeManager::GetDiagnostics(const void* owner) const
	{
		ReflectionProbeDiagnostics diagnostics{};
		const ProbeRuntime* probe = FindProbe(owner);
		if (!probe) return diagnostics;
		diagnostics.registered = true;
		diagnostics.captured = probe->captured;
		diagnostics.dirty = probe->dirty;
		diagnostics.captureRevision = probe->captureRevision;
		diagnostics.resolution = probe->target ? probe->target->resolution : probe->desc.resolution;
		return diagnostics;
	}

	inline uint32_t ReflectionProbeManager::GetCapturedProbeCount() const
	{
		uint32_t count = 0;
		for (const ProbeRuntime& probe : probes_)
		{
			if (probe.owner && probe.desc.enabled && probe.captured) ++count;
		}
		return count;
	}
} // namespace Ken4lowEngine
