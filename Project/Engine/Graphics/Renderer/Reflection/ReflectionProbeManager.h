#pragma once

#include "DX12Include.h"
#include "Vector3.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;
	class SkyBox;

	enum class ReflectionProbeUpdateMode : uint8_t
	{
		Static = 0,
		OnDemand,
		EveryFrame,
	};

	struct ReflectionProbeDesc
	{
		Vector3 position{};
		float influenceRadius = 10.0f;
		float nearClip = 0.1f;
		float farClip = 100.0f;
		uint32_t resolution = 256;
		ReflectionProbeUpdateMode updateMode = ReflectionProbeUpdateMode::Static;
		bool enabled = true;
	};

	struct ReflectionProbeDiagnostics
	{
		bool registered = false;
		bool captured = false;
		bool dirty = false;
		uint64_t captureRevision = 0;
		uint32_t resolution = 0;
	};

	/// <summary>
	/// Scene内の局所Reflection Probeを管理し、近傍Probeが無い場合だけEnvironment Mapへフォールバックします。
	/// </summary>
	class ReflectionProbeManager
	{
	public:
		static ReflectionProbeManager* GetInstance();

		void Initialize(DirectXCommon* dxCommon);
		void Finalize();

		void RegisterOrUpdateProbe(const void* owner, const ReflectionProbeDesc& desc, bool forceDirty = false);
		void UnregisterProbe(const void* owner);
		void RequestCapture(const void* owner);

		/// 1フレーム最大1Probeだけ更新し、各Probeは6面を連続Captureします。
		bool CapturePending(const std::function<void()>& drawStaticScene);

		D3D12_GPU_DESCRIPTOR_HANDLE ResolveReflectionHandle(const Vector3& worldPosition) const;
		ReflectionProbeDiagnostics GetDiagnostics(const void* owner) const;
		uint32_t GetCapturedProbeCount() const;
		bool IsCapturing() const { return isCapturing_; }

	private:
		struct ProbeTarget
		{
			ComPtr<ID3D12Resource> color;
			ComPtr<ID3D12Resource> depth;
			std::array<uint32_t, 6> rtvIndices{
				UINT32_MAX, UINT32_MAX, UINT32_MAX,
				UINT32_MAX, UINT32_MAX, UINT32_MAX
			};
			uint32_t dsvIndex = UINT32_MAX;
			uint32_t srvIndex = UINT32_MAX;
			uint32_t resolution = 0;
			D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			D3D12_VIEWPORT viewport{};
			D3D12_RECT scissor{};
		};

		struct ProbeRuntime
		{
			const void* owner = nullptr;
			ReflectionProbeDesc desc{};
			std::unique_ptr<ProbeTarget> target;
			bool captured = false;
			bool dirty = true;
			uint64_t captureRevision = 0;
		};

		ReflectionProbeManager() = default;
		~ReflectionProbeManager() = default;
		ReflectionProbeManager(const ReflectionProbeManager&) = delete;
		ReflectionProbeManager& operator=(const ReflectionProbeManager&) = delete;

		ReflectionProbeDesc SanitizeDesc(const ReflectionProbeDesc& desc) const;
		ProbeRuntime* FindProbe(const void* owner);
		const ProbeRuntime* FindProbe(const void* owner) const;
		ProbeRuntime* FindCaptureCandidate();
		bool EnsureTarget(ProbeRuntime& probe);
		std::unique_ptr<ProbeTarget> CreateTarget(uint32_t resolution);
		void BeginFace(ProbeTarget& target, uint32_t faceIndex);
		void EndCapture(ProbeTarget& target);
		void ReleaseTarget(ProbeTarget& target);
		void TransitionTarget(ProbeTarget& target, D3D12_RESOURCE_STATES nextState);
		bool CaptureProbe(ProbeRuntime& probe, const std::function<void()>& drawStaticScene);
		void SyncCaptureSkyBox();

		DirectXCommon* dxCommon_ = nullptr;
		std::vector<ProbeRuntime> probes_{};
		std::vector<std::unique_ptr<ProbeTarget>> retiredTargets_{}; // GPU参照中Descriptorを同Frameで再利用しないためResize旧Targetを保持する。
		std::unique_ptr<SkyBox> captureSkyBox_{};
		bool isCapturing_ = false;
		uint64_t captureSerial_ = 0;
	};
} // namespace Ken4lowEngine

#include "ReflectionProbeManager.inl"
