#pragma once

#include "DX12Include.h"
#include "Matrix4x4.h"
#include "Vector3.h"
#include "Vector4.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	class Actor;
	class DirectXCommon;

	inline constexpr uint32_t kMaxPlanarReflectionSurfacesPerDraw = 6u;

	enum class PlanarReflectionUpdateMode : uint8_t
	{
		OnDemand = 0,
		EveryFrame,
	};

	struct PlanarReflectionDesc
	{
		Vector3 position{};
		Vector3 normal{ 0.0f, 1.0f, 0.0f };
		float strength = 1.0f;
		float surfaceTolerance = 0.025f;
		float clipPlaneBias = 0.01f;
		PlanarReflectionUpdateMode updateMode = PlanarReflectionUpdateMode::EveryFrame;
		bool enabled = true;
	};

	struct PlanarReflectionBinding
	{
		D3D12_GPU_DESCRIPTOR_HANDLE texture{};
		uint32_t srvIndex = UINT32_MAX;
		Matrix4x4 reflectedViewProjection = Matrix4x4::MakeIdentity();
		Vector3 planePosition{};
		Vector3 planeNormal{ 0.0f, 1.0f, 0.0f };
		float surfaceTolerance = 0.025f;
		float strength = 0.0f;
		bool valid = false;
	};

	struct PlanarReflectionDrawSet
	{
		std::array<PlanarReflectionBinding, kMaxPlanarReflectionSurfacesPerDraw> surfaces{};
		uint32_t count = 0;

		bool Add(const PlanarReflectionBinding& binding)
		{
			if (!binding.valid || binding.texture.ptr == 0 || binding.srvIndex == UINT32_MAX ||
				count >= kMaxPlanarReflectionSurfacesPerDraw)
			{
				return false;
			}
			surfaces[count++] = binding;
			return true; // 1 Drawで最大6面まで同時に鏡像を選択できるよう固定長Packetへ積む。
		}

		bool Empty() const { return count == 0; }
	};

	struct PlanarReflectionDrawCBData
	{
		uint32_t surfaceCount = 0;
		float padding[3]{};
		std::array<Matrix4x4, kMaxPlanarReflectionSurfacesPerDraw> reflectedViewProjection{};
		std::array<Vector4, kMaxPlanarReflectionSurfacesPerDraw> plane{};
		std::array<Vector4, kMaxPlanarReflectionSurfacesPerDraw> surfaceParams{}; // x=Tolerance, y=Strength
	};

	struct PlanarReflectionDiagnostics
	{
		bool registered = false;
		bool captured = false;
		bool dirty = false;
		bool obliqueClipApplied = false;
		uint64_t captureRevision = 0;
	};

	/// <summary>
	/// 鏡面ごとの反射RenderTargetと反射Camera Captureを管理します。
	/// Main Viewportと同じ解像度で1フレーム最大1面だけ更新します。
	/// </summary>
	class PlanarReflectionManager
	{
	public:
		class ScopedDrawBinding
		{
		public:
			ScopedDrawBinding(PlanarReflectionManager* manager, const PlanarReflectionBinding& binding);
			ScopedDrawBinding(PlanarReflectionManager* manager, const PlanarReflectionDrawSet& drawSet);
			~ScopedDrawBinding();

			ScopedDrawBinding(const ScopedDrawBinding&) = delete;
			ScopedDrawBinding& operator=(const ScopedDrawBinding&) = delete;

		private:
			PlanarReflectionManager* manager_ = nullptr;
			bool active_ = false;
		};

		static PlanarReflectionManager* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		void RegisterOrUpdateSurface(
			const void* owner,
			const Actor* receiverActor,
			const PlanarReflectionDesc& desc,
			bool forceDirty = false);
		void UnregisterSurface(const void* owner);
		void RequestCapture(const void* owner);

		/// <summary>
		/// 更新対象をラウンドロビンで1面だけ選び、反射CameraからSceneを描画します。
		/// callbackには鏡自身として除外するActorを渡します。
		/// </summary>
		bool CapturePending(const std::function<void(const Actor*)>& drawScene);

		PlanarReflectionBinding ResolveBinding(const void* owner) const;
		PlanarReflectionBinding GetCurrentDrawBinding() const;
		PlanarReflectionDrawSet GetCurrentDrawSet() const;
		bool BindCurrentDrawState(
			ID3D12GraphicsCommandList* commandList,
			UINT constantBufferRootParameterIndex,
			UINT textureTableRootParameterIndex) const;
		bool BindCurrentDrawDescriptorTable(ID3D12GraphicsCommandList* commandList, UINT rootParameterIndex) const;
		PlanarReflectionDiagnostics GetDiagnostics(const void* owner) const;
		bool IsCapturing() const { return isCapturing_; }

	private:
		struct SurfaceTarget
		{
			ComPtr<ID3D12Resource> color;
			ComPtr<ID3D12Resource> depth;
			uint32_t rtvIndex = UINT32_MAX;
			uint32_t dsvIndex = UINT32_MAX;
			uint32_t srvIndex = UINT32_MAX;
			D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			D3D12_VIEWPORT viewport{};
			D3D12_RECT scissor{};
		};

		struct SurfaceRuntime
		{
			const void* owner = nullptr;
			const Actor* receiverActor = nullptr;
			PlanarReflectionDesc desc{};
			std::unique_ptr<SurfaceTarget> target;
			Matrix4x4 capturedViewProjection = Matrix4x4::MakeIdentity();
			bool captured = false;
			bool dirty = true;
			bool obliqueClipApplied = false;
			uint64_t captureRevision = 0;
		};

		PlanarReflectionManager() = default;
		~PlanarReflectionManager() = default;
		PlanarReflectionManager(const PlanarReflectionManager&) = delete;
		PlanarReflectionManager& operator=(const PlanarReflectionManager&) = delete;

		PlanarReflectionDesc SanitizeDesc(const PlanarReflectionDesc& desc) const;
		SurfaceRuntime* FindSurface(const void* owner);
		const SurfaceRuntime* FindSurface(const void* owner) const;
		SurfaceRuntime* FindCaptureCandidate();
		bool EnsureTarget(SurfaceRuntime& surface);
		std::unique_ptr<SurfaceTarget> CreateTarget();
		void ReleaseTarget(SurfaceTarget& target);
		void TransitionTarget(SurfaceTarget& target, D3D12_RESOURCE_STATES nextState);
		void BeginTarget(SurfaceTarget& target);
		void EndTarget(SurfaceTarget& target);
		bool CaptureSurface(SurfaceRuntime& surface, const std::function<void(const Actor*)>& drawScene);
		void PushDrawBinding(const PlanarReflectionDrawSet& drawSet);
		void PopDrawBinding();

		DirectXCommon* dxCommon_ = nullptr;
		std::vector<SurfaceRuntime> surfaces_{};
		std::vector<PlanarReflectionDrawSet> drawBindings_{};
		std::size_t captureCursor_ = 0;
		uint64_t captureSerial_ = 0;
		bool isCapturing_ = false;
	};
} // namespace Ken4lowEngine

#ifdef max
#pragma push_macro("max")
#undef max
#define KEN4LOW_PLANAR_REFLECTION_RESTORE_MAX
#endif

#ifdef min
#pragma push_macro("min")
#undef min
#define KEN4LOW_PLANAR_REFLECTION_RESTORE_MIN
#endif

#include "PlanarReflectionManager.inl" // Windowsのmin/maxマクロを隔離して反射Camera計算を安全にする。

#ifdef KEN4LOW_PLANAR_REFLECTION_RESTORE_MIN
#pragma pop_macro("min")
#undef KEN4LOW_PLANAR_REFLECTION_RESTORE_MIN
#endif

#ifdef KEN4LOW_PLANAR_REFLECTION_RESTORE_MAX
#pragma pop_macro("max")
#undef KEN4LOW_PLANAR_REFLECTION_RESTORE_MAX
#endif