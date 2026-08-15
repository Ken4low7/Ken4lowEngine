#include "CameraManager.h"
#include "DirectXCommon.h"
#include "DSVManager.h"
#include "GameViewportConstants.h"
#include "RTVManager.h"
#include "SRVManager.h"
#include "Engine/Graphics/Renderer/Object3D/Object3DCommon.h"

#include <algorithm>
#include <cmath>

namespace Ken4lowEngine
{
	static_assert(sizeof(PlanarReflectionDrawCBData) == 592, "PlanarReflectionDrawCBData must match the HLSL b7 layout.");

	namespace PlanarReflectionDetail
	{
		struct HomogeneousVector
		{
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			float w = 0.0f;
		};

		inline bool NearlyEqual(float lhs, float rhs)
		{
			return std::fabs(lhs - rhs) <= 0.0001f;
		}

		inline bool SameVector(const Vector3& lhs, const Vector3& rhs)
		{
			return NearlyEqual(lhs.x, rhs.x) &&
				NearlyEqual(lhs.y, rhs.y) &&
				NearlyEqual(lhs.z, rhs.z);
		}

		inline Vector3 ReflectVector(const Vector3& value, const Vector3& normal)
		{
			return value - normal * (2.0f * Vector3::Dot(value, normal));
		}

		inline Vector3 ReflectPoint(const Vector3& point, const Vector3& planePoint, const Vector3& planeNormal)
		{
			const float signedDistance = Vector3::Dot(point - planePoint, planeNormal);
			return point - planeNormal * (2.0f * signedDistance);
		}

		inline Vector3 TransformDirection(const Vector3& direction, const Matrix4x4& matrix)
		{
			return {
				direction.x * matrix.m[0][0] + direction.y * matrix.m[1][0] + direction.z * matrix.m[2][0],
				direction.x * matrix.m[0][1] + direction.y * matrix.m[1][1] + direction.z * matrix.m[2][1],
				direction.x * matrix.m[0][2] + direction.y * matrix.m[1][2] + direction.z * matrix.m[2][2],
			};
		}

		inline HomogeneousVector TransformHomogeneousRow(const HomogeneousVector& value, const Matrix4x4& matrix)
		{
			return {
				value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + value.w * matrix.m[3][0],
				value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + value.w * matrix.m[3][1],
				value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + value.w * matrix.m[3][2],
				value.x * matrix.m[0][3] + value.y * matrix.m[1][3] + value.z * matrix.m[2][3] + value.w * matrix.m[3][3],
			};
		}

		inline float Dot(const HomogeneousVector& lhs, const HomogeneousVector& rhs)
		{
			return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z + lhs.w * rhs.w;
		}

		inline float SignNotZero(float value)
		{
			return value >= 0.0f ? 1.0f : -1.0f;
		}

		inline Matrix4x4 BuildObliqueProjection(
			const Matrix4x4& projection,
			const Matrix4x4& view,
			const Vector3& planePosition,
			const Vector3& keepSideNormal,
			float clipPlaneBias,
			bool& outApplied)
		{
			outApplied = false;
			const Vector3 worldNormal = Vector3::NormalizeSafe(keepSideNormal, { 0.0f, 1.0f, 0.0f });
			const Vector3 biasedPlanePosition = planePosition + worldNormal * (std::max)(clipPlaneBias, 0.0f);
			const Vector3 planePointView = Vector3::Transform(biasedPlanePosition, view);
			const Vector3 planeNormalView = Vector3::NormalizeSafe(
				TransformDirection(worldNormal, view),
				{ 0.0f, 0.0f, 1.0f });

			HomogeneousVector planeView{
				planeNormalView.x,
				planeNormalView.y,
				planeNormalView.z,
				-Vector3::Dot(planeNormalView, planePointView),
			};

			// keepSideNormal側がDirectXのnear条件 clip.z >= 0 になる向きへ揃える。
			const Vector3 keepPointView = Vector3::Transform(biasedPlanePosition + worldNormal, view);
			const float keepSideValue =
				planeView.x * keepPointView.x +
				planeView.y * keepPointView.y +
				planeView.z * keepPointView.z +
				planeView.w;
			if (keepSideValue < 0.0f)
			{
				planeView.x = -planeView.x;
				planeView.y = -planeView.y;
				planeView.z = -planeView.z;
				planeView.w = -planeView.w;
			}

			Matrix4x4 inverseProjection{};
			if (!Matrix4x4::TryInverse(projection, inverseProjection))
			{
				return projection;
			}

			const HomogeneousVector farClipCorner{
				SignNotZero(planeView.x),
				SignNotZero(planeView.y),
				1.0f,
				1.0f,
			};
			const HomogeneousVector farViewCorner = TransformHomogeneousRow(farClipCorner, inverseProjection);
			const float denominator = Dot(planeView, farViewCorner);
			if (std::fabs(denominator) <= 1.0e-6f)
			{
				return projection;
			}

			const float scale = farClipCorner.w / denominator;
			Matrix4x4 oblique = projection;
			oblique.m[0][2] = planeView.x * scale;
			oblique.m[1][2] = planeView.y * scale;
			oblique.m[2][2] = planeView.z * scale;
			oblique.m[3][2] = planeView.w * scale;
			outApplied = true; // DirectX row-vector規約ではZ列を任意near planeへ置換して鏡裏側をGPU Clipする。
			return oblique;
		}
	}

	inline PlanarReflectionManager::ScopedDrawBinding::ScopedDrawBinding(
		PlanarReflectionManager* manager,
		const PlanarReflectionBinding& binding)
		: manager_(manager)
	{
		PlanarReflectionDrawSet drawSet{};
		if (manager_ && drawSet.Add(binding))
		{
			manager_->PushDrawBinding(drawSet);
			active_ = true;
		}
	}

	inline PlanarReflectionManager::ScopedDrawBinding::ScopedDrawBinding(
		PlanarReflectionManager* manager,
		const PlanarReflectionDrawSet& drawSet)
		: manager_(manager)
	{
		if (manager_ && !drawSet.Empty())
		{
			manager_->PushDrawBinding(drawSet);
			active_ = true;
		}
	}

	inline PlanarReflectionManager::ScopedDrawBinding::~ScopedDrawBinding()
	{
		if (active_ && manager_)
		{
			manager_->PopDrawBinding();
		}
	}

	inline PlanarReflectionManager* PlanarReflectionManager::GetInstance()
	{
		static PlanarReflectionManager instance;
		return &instance;
	}

	inline void PlanarReflectionManager::Initialize(DirectXCommon* dxCommon)
	{
		if (dxCommon_ == dxCommon && dxCommon_ != nullptr)
		{
			return;
		}
		Finalize();
		dxCommon_ = dxCommon;
		captureCursor_ = 0;
		captureSerial_ = 0;
		isCapturing_ = false;
	}

	inline void PlanarReflectionManager::Finalize()
	{
		for (SurfaceRuntime& surface : surfaces_)
		{
			if (surface.target)
			{
				ReleaseTarget(*surface.target);
			}
		}
		surfaces_.clear();
		drawBindings_.clear();
		captureCursor_ = 0;
		captureSerial_ = 0;
		isCapturing_ = false;
		dxCommon_ = nullptr;
	}

	inline PlanarReflectionDesc PlanarReflectionManager::SanitizeDesc(const PlanarReflectionDesc& desc) const
	{
		PlanarReflectionDesc sanitized = desc;
		sanitized.normal = Vector3::NormalizeSafe(desc.normal, { 0.0f, 1.0f, 0.0f });
		sanitized.strength = std::clamp(desc.strength, 0.0f, 1.0f);
		sanitized.surfaceTolerance = std::clamp(desc.surfaceTolerance, 0.001f, 1.0f);
		sanitized.clipPlaneBias = std::clamp(desc.clipPlaneBias, 0.0f, 1.0f);
		return sanitized;
	}

	inline void PlanarReflectionManager::RegisterOrUpdateSurface(
		const void* owner,
		const Actor* receiverActor,
		const PlanarReflectionDesc& desc,
		bool forceDirty)
	{
		if (!owner) return;
		const PlanarReflectionDesc sanitized = SanitizeDesc(desc);
		SurfaceRuntime* surface = FindSurface(owner);
		if (!surface)
		{
			SurfaceRuntime runtime{};
			runtime.owner = owner;
			runtime.receiverActor = receiverActor;
			runtime.desc = sanitized;
			runtime.dirty = true;
			surfaces_.push_back(std::move(runtime));
			return;
		}

		const bool cameraInputChanged =
			!PlanarReflectionDetail::SameVector(surface->desc.position, sanitized.position) ||
			!PlanarReflectionDetail::SameVector(surface->desc.normal, sanitized.normal) ||
			!PlanarReflectionDetail::NearlyEqual(surface->desc.surfaceTolerance, sanitized.surfaceTolerance) ||
			!PlanarReflectionDetail::NearlyEqual(surface->desc.clipPlaneBias, sanitized.clipPlaneBias) ||
			surface->desc.enabled != sanitized.enabled ||
			surface->receiverActor != receiverActor;
		surface->receiverActor = receiverActor;
		surface->desc = sanitized;
		surface->dirty = surface->dirty || cameraInputChanged || forceDirty;
	}

	inline void PlanarReflectionManager::UnregisterSurface(const void* owner)
	{
		SurfaceRuntime* surface = FindSurface(owner);
		if (!surface) return;
		surface->owner = nullptr;
		surface->receiverActor = nullptr;
		surface->desc.enabled = false;
		surface->dirty = false; // GPU参照中のRT/SRVを即解放せずReflection System終了まで寿命を保つ。
	}

	inline void PlanarReflectionManager::RequestCapture(const void* owner)
	{
		if (SurfaceRuntime* surface = FindSurface(owner))
		{
			surface->dirty = true;
		}
	}

	inline PlanarReflectionManager::SurfaceRuntime* PlanarReflectionManager::FindSurface(const void* owner)
	{
		if (!owner) return nullptr;
		const auto found = std::find_if(surfaces_.begin(), surfaces_.end(),
			[owner](const SurfaceRuntime& surface) { return surface.owner == owner; });
		return found != surfaces_.end() ? &*found : nullptr;
	}

	inline const PlanarReflectionManager::SurfaceRuntime* PlanarReflectionManager::FindSurface(const void* owner) const
	{
		if (!owner) return nullptr;
		const auto found = std::find_if(surfaces_.begin(), surfaces_.end(),
			[owner](const SurfaceRuntime& surface) { return surface.owner == owner; });
		return found != surfaces_.end() ? &*found : nullptr;
	}

	inline PlanarReflectionManager::SurfaceRuntime* PlanarReflectionManager::FindCaptureCandidate()
	{
		if (surfaces_.empty()) return nullptr;
		const std::size_t count = surfaces_.size();
		for (std::size_t offset = 0; offset < count; ++offset)
		{
			const std::size_t index = (captureCursor_ + offset) % count;
			SurfaceRuntime& surface = surfaces_[index];
			if (!surface.owner || !surface.receiverActor || !surface.desc.enabled) continue;
			if (surface.desc.updateMode == PlanarReflectionUpdateMode::EveryFrame)
			{
				surface.dirty = true; // Planar ReflectionはCamera依存なので動的鏡は毎フレーム再Captureする。
			}
			if (!surface.dirty && surface.captured) continue;
			captureCursor_ = (index + 1) % count;
			return &surface;
		}
		return nullptr;
	}

	inline bool PlanarReflectionManager::CapturePending(const std::function<void(const Actor*)>& drawScene)
	{
		if (!dxCommon_ || isCapturing_ || !drawScene) return false;
		SurfaceRuntime* surface = FindCaptureCandidate();
		if (!surface) return false;
		return CaptureSurface(*surface, drawScene); // 1フレーム1面に制限し、6面鏡でもCapture負荷を予算化する。
	}

	inline bool PlanarReflectionManager::EnsureTarget(SurfaceRuntime& surface)
	{
		if (surface.target && surface.target->color && surface.target->depth)
		{
			return true;
		}
		surface.target = CreateTarget();
		return surface.target != nullptr;
	}

	inline std::unique_ptr<PlanarReflectionManager::SurfaceTarget> PlanarReflectionManager::CreateTarget()
	{
		if (!dxCommon_ || !dxCommon_->GetDevice()) return nullptr;
		auto target = std::make_unique<SurfaceTarget>();

		D3D12_RESOURCE_DESC colorDesc{};
		colorDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		colorDesc.Width = GameViewportConstants::Width;
		colorDesc.Height = GameViewportConstants::Height;
		colorDesc.DepthOrArraySize = 1;
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
		target->color->SetName(L"PlanarReflectionColor");

		target->rtvIndex = RTVManager::GetInstance()->Allocate();
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		rtvDesc.Texture2D.PlaneSlice = 0;
		dxCommon_->GetDevice()->CreateRenderTargetView(
			target->color.Get(), &rtvDesc, RTVManager::GetInstance()->GetCPUDescriptorHandle(target->rtvIndex));

		D3D12_CLEAR_VALUE depthClear{};
		target->depth = DSVManager::GetInstance()->CreateDepthStencilBuffer(
			GameViewportConstants::Width,
			GameViewportConstants::Height,
			DXGI_FORMAT_D24_UNORM_S8_UINT,
			depthClear);
		if (!target->depth)
		{
			ReleaseTarget(*target);
			return nullptr;
		}
		target->depth->SetName(L"PlanarReflectionDepth");
		target->dsvIndex = DSVManager::GetInstance()->Allocate();
		DSVManager::GetInstance()->CreateDSVForDepthBuffer(target->dsvIndex, target->depth.Get());

		target->srvIndex = SRVManager::GetInstance()->Allocate();
		SRVManager::GetInstance()->CreateSRVForTexture2D(
			target->srvIndex,
			target->color.Get(),
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			1);

		target->state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		target->viewport = D3D12_VIEWPORT(
			0.0f,
			0.0f,
			static_cast<float>(GameViewportConstants::Width),
			static_cast<float>(GameViewportConstants::Height),
			0.0f,
			1.0f);
		target->scissor = {
			0,
			0,
			static_cast<LONG>(GameViewportConstants::Width),
			static_cast<LONG>(GameViewportConstants::Height)
		};
		return target;
	}

	inline void PlanarReflectionManager::ReleaseTarget(SurfaceTarget& target)
	{
		if (target.rtvIndex != UINT32_MAX)
		{
			RTVManager::GetInstance()->Free(target.rtvIndex);
			target.rtvIndex = UINT32_MAX;
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

	inline void PlanarReflectionManager::TransitionTarget(SurfaceTarget& target, D3D12_RESOURCE_STATES nextState)
	{
		if (!target.color || target.state == nextState) return;
		dxCommon_->ResourceTransition(target.color.Get(), target.state, nextState);
		target.state = nextState;
	}

	inline void PlanarReflectionManager::BeginTarget(SurfaceTarget& target)
	{
		TransitionTarget(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
		ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandManager()->GetCommandList();
		commandList->RSSetViewports(1, &target.viewport);
		commandList->RSSetScissorRects(1, &target.scissor);
		const D3D12_CPU_DESCRIPTOR_HANDLE rtv = RTVManager::GetInstance()->GetCPUDescriptorHandle(target.rtvIndex);
		const D3D12_CPU_DESCRIPTOR_HANDLE dsv = DSVManager::GetInstance()->GetCPUDescriptorHandle(target.dsvIndex);
		commandList->OMSetRenderTargets(1, &rtv, false, &dsv);
		constexpr float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(
			dsv,
			D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
			1.0f,
			0,
			0,
			nullptr);
	}

	inline void PlanarReflectionManager::EndTarget(SurfaceTarget& target)
	{
		TransitionTarget(target, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	}

	inline bool PlanarReflectionManager::CaptureSurface(
		SurfaceRuntime& surface,
		const std::function<void(const Actor*)>& drawScene)
	{
		if (!EnsureTarget(surface) || !surface.target) return false;

		CameraManager* cameraManager = CameraManager::GetInstance();
		Object3DCommon* objectCommon = Object3DCommon::GetInstance();
		const Vector3 planeNormal = Vector3::NormalizeSafe(surface.desc.normal, { 0.0f, 1.0f, 0.0f });
		const Vector3 cameraPosition = cameraManager->GetActiveCameraPosition();
		const Vector3 cameraForward = Vector3::NormalizeSafe(cameraManager->GetActiveCameraForward(), { 0.0f, 0.0f, 1.0f });

		Vector3 referenceUp{ 0.0f, 1.0f, 0.0f };
		if (std::fabs(Vector3::Dot(referenceUp, cameraForward)) > 0.98f)
		{
			referenceUp = { 0.0f, 0.0f, 1.0f };
		}
		const Vector3 cameraRight = Vector3::NormalizeSafe(Vector3::Cross(referenceUp, cameraForward), { 1.0f, 0.0f, 0.0f });
		const Vector3 cameraUp = Vector3::NormalizeSafe(Vector3::Cross(cameraForward, cameraRight), { 0.0f, 1.0f, 0.0f });
		const Vector3 reflectedPosition = PlanarReflectionDetail::ReflectPoint(cameraPosition, surface.desc.position, planeNormal);
		const Vector3 reflectedForward = Vector3::NormalizeSafe(
			PlanarReflectionDetail::ReflectVector(cameraForward, planeNormal),
			{ 0.0f, 0.0f, 1.0f });
		const Vector3 reflectedUp = Vector3::NormalizeSafe(
			PlanarReflectionDetail::ReflectVector(cameraUp, planeNormal),
			{ 0.0f, 1.0f, 0.0f });

		CameraManager::RenderViewOverride reflectedView{};
		reflectedView.view = Matrix4x4::LookAt(
			reflectedPosition,
			reflectedPosition + reflectedForward,
			reflectedUp);

		Vector3 keepSideNormal = planeNormal;
		if (Vector3::Dot(cameraPosition - surface.desc.position, keepSideNormal) < 0.0f)
		{
			keepSideNormal = -keepSideNormal; // 鏡面法線の設定向きに依存せず、Main Cameraと同じ側だけをReflectionへ残す。
		}
		bool obliqueClipApplied = false;
		reflectedView.projection = PlanarReflectionDetail::BuildObliqueProjection(
			cameraManager->GetActiveProjectionMatrix(),
			reflectedView.view,
			surface.desc.position,
			keepSideNormal,
			surface.desc.clipPlaneBias,
			obliqueClipApplied);
		reflectedView.viewProjection = Matrix4x4::Multiply(reflectedView.view, reflectedView.projection);
		reflectedView.position = reflectedPosition;
		reflectedView.forward = reflectedForward;
		reflectedView.nearClip = cameraManager->GetActiveNearClip();
		reflectedView.farClip = cameraManager->GetActiveFarClip();
		reflectedView.fovY = cameraManager->GetActiveFovY();
		reflectedView.aspectRatio = cameraManager->GetActiveAspectRatio();

		const Object3DCommon::CullingCameraMode previousCullingMode = objectCommon->GetCullingCameraMode();
		objectCommon->SetCullingCameraMode(Object3DCommon::CullingCameraMode::ActiveCamera);
		isCapturing_ = true;
		cameraManager->PushRenderViewOverride(reflectedView);
		BeginTarget(*surface.target);
		SRVManager::GetInstance()->PreDraw();
		objectCommon->BeginObject3DPass();

		bool succeeded = true;
		try
		{
			drawScene(surface.receiverActor); // 鏡自身を除外し、鏡裏側はOblique Near Planeで三角形単位にClipする。
		}
		catch (...)
		{
			succeeded = false;
		}

		objectCommon->EndObject3DPass();
		EndTarget(*surface.target);
		cameraManager->PopRenderViewOverride();
		objectCommon->SetCullingCameraMode(previousCullingMode);
		isCapturing_ = false;

		if (!succeeded) return false;
		surface.capturedViewProjection = reflectedView.viewProjection; // Captureに実際に使ったOblique Projection込み行列をMain描画でも再利用する。
		surface.obliqueClipApplied = obliqueClipApplied;
		surface.captured = true;
		surface.dirty = false;
		surface.captureRevision = ++captureSerial_;
		return true;
	}

	inline PlanarReflectionBinding PlanarReflectionManager::ResolveBinding(const void* owner) const
	{
		PlanarReflectionBinding binding{};
		if (isCapturing_) return binding;
		const SurfaceRuntime* surface = FindSurface(owner);
		if (!surface || !surface->owner || !surface->desc.enabled || !surface->captured || !surface->target) return binding;
		if (surface->target->srvIndex == UINT32_MAX) return binding;
		binding.srvIndex = surface->target->srvIndex;
		binding.texture = SRVManager::GetInstance()->GetGPUDescriptorHandle(binding.srvIndex);
		binding.reflectedViewProjection = surface->capturedViewProjection;
		binding.planePosition = surface->desc.position;
		binding.planeNormal = surface->desc.normal;
		binding.surfaceTolerance = surface->desc.surfaceTolerance;
		binding.strength = surface->desc.strength;
		binding.valid = binding.texture.ptr != 0;
		return binding;
	}

	inline PlanarReflectionBinding PlanarReflectionManager::GetCurrentDrawBinding() const
	{
		const PlanarReflectionDrawSet drawSet = GetCurrentDrawSet();
		return drawSet.count > 0 ? drawSet.surfaces[0] : PlanarReflectionBinding{};
	}

	inline PlanarReflectionDrawSet PlanarReflectionManager::GetCurrentDrawSet() const
	{
		return drawBindings_.empty() ? PlanarReflectionDrawSet{} : drawBindings_.back();
	}

	inline bool PlanarReflectionManager::BindCurrentDrawState(
		ID3D12GraphicsCommandList* commandList,
		UINT constantBufferRootParameterIndex,
		UINT textureTableRootParameterIndex) const
	{
		if (!commandList || !dxCommon_) return false;
		const PlanarReflectionDrawSet drawSet = GetCurrentDrawSet();
		PlanarReflectionDrawCBData drawData{};
		drawData.surfaceCount = (std::min)(drawSet.count, kMaxPlanarReflectionSurfacesPerDraw);
		for (uint32_t index = 0; index < drawData.surfaceCount; ++index)
		{
			const PlanarReflectionBinding& binding = drawSet.surfaces[index];
			const Vector3 normal = Vector3::NormalizeSafe(binding.planeNormal, { 0.0f, 1.0f, 0.0f });
			drawData.reflectedViewProjection[index] = binding.reflectedViewProjection;
			drawData.plane[index] = {
				normal.x,
				normal.y,
				normal.z,
				-Vector3::Dot(normal, binding.planePosition),
			};
			drawData.surfaceParams[index] = {
				std::clamp(binding.surfaceTolerance, 0.001f, 1.0f),
				std::clamp(binding.strength, 0.0f, 1.0f),
				0.0f,
				0.0f,
			};
		}

		const FrameUploadArena::Allocation allocation = dxCommon_->GetFrameUploadArena().AllocateConstant(drawData);
		if (!allocation.IsValid()) return false;
		commandList->SetGraphicsRootConstantBufferView(constantBufferRootParameterIndex, allocation.gpuAddress);
		if (drawData.surfaceCount == 0) return true;
		return BindCurrentDrawDescriptorTable(commandList, textureTableRootParameterIndex);
	}

	inline bool PlanarReflectionManager::BindCurrentDrawDescriptorTable(
		ID3D12GraphicsCommandList* commandList,
		UINT rootParameterIndex) const
	{
		if (!commandList || !dxCommon_ || !dxCommon_->GetDevice() || drawBindings_.empty()) return false;
		const PlanarReflectionDrawSet& drawSet = drawBindings_.back();
		if (drawSet.Empty()) return false;

		SRVManager* srvManager = SRVManager::GetInstance();
		const SRVManager::TransientDescriptorAllocation allocation =
			srvManager->AllocateTransient(kMaxPlanarReflectionSurfacesPerDraw);
		if (!allocation.IsValid()) return false;

		const uint32_t fallbackSrvIndex = drawSet.surfaces[0].srvIndex;
		const uint32_t descriptorSize = srvManager->GetDescriptorSize();
		for (uint32_t index = 0; index < kMaxPlanarReflectionSurfacesPerDraw; ++index)
		{
			const uint32_t sourceSrvIndex =
				index < drawSet.count && drawSet.surfaces[index].srvIndex != UINT32_MAX
				? drawSet.surfaces[index].srvIndex
				: fallbackSrvIndex;
			D3D12_CPU_DESCRIPTOR_HANDLE destination = allocation.cpuHandle;
			destination.ptr += static_cast<SIZE_T>(descriptorSize) * index;
			dxCommon_->GetDevice()->CopyDescriptorsSimple(
				1,
				destination,
				srvManager->GetCPUDescriptorHandle(sourceSrvIndex),
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, allocation.gpuHandle);
		return true; // 永続SRVをFrame Transientの連続6枠へ複製し、t12～t17を1 Tableで束縛する。
	}

	inline PlanarReflectionDiagnostics PlanarReflectionManager::GetDiagnostics(const void* owner) const
	{
		PlanarReflectionDiagnostics diagnostics{};
		const SurfaceRuntime* surface = FindSurface(owner);
		if (!surface) return diagnostics;
		diagnostics.registered = true;
		diagnostics.captured = surface->captured;
		diagnostics.dirty = surface->dirty;
		diagnostics.obliqueClipApplied = surface->obliqueClipApplied;
		diagnostics.captureRevision = surface->captureRevision;
		return diagnostics;
	}

	inline void PlanarReflectionManager::PushDrawBinding(const PlanarReflectionDrawSet& drawSet)
	{
		if (!drawSet.Empty())
		{
			drawBindings_.push_back(drawSet);
		}
	}

	inline void PlanarReflectionManager::PopDrawBinding()
	{
		if (!drawBindings_.empty())
		{
			drawBindings_.pop_back();
		}
	}
} // namespace Ken4lowEngine