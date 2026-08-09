#pragma once

#include "DX12Include.h"
#include "LightManager.h"
#include <Engine/Graphics/Device/Buffer/PerFrameUploadBuffer.h>

#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{
	class DirectXCommon;

	class LightGpuBuffer
	{
	public:
		void Initialize(DirectXCommon* dxCommon, const LightManager::LightingSettingsGPU& initialLightingSettings);
		void Finalize();

		void UpdatePunctualLights(
			const std::vector<LightManager::PunctualLightGPU>& punctualLights,
			const std::vector<LightManager::PunctualLightGPU>& lightComponentLights);

		void BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2);
		void BindLightingSettings(uint32_t rootIndexCB_b5, const LightManager::LightingSettingsGPU& lightingSettings);

	private:
		struct PunctualFrameBuffer
		{
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			uint32_t bufferBytes = 0;
			uint32_t srvIndex = UINT32_MAX;
			bool srvAllocated = false;
		};

		uint32_t GetCurrentFrameIndex() const;
		void EnsurePunctualFrameCapacity(uint32_t frameIndex, uint32_t bytes, uint32_t elementCount);
		void UploadPunctualLightsForFrame(uint32_t frameIndex);

	private:
		DirectXCommon* dxCommon_ = nullptr;
		PerFrameUploadBuffer<LightManager::LightInfo> lightInfoBuffers_;
		PerFrameUploadBuffer<LightManager::LightingSettingsGPU> lightingSettingsBuffers_;
		LightManager::LightInfo lightInfoCpu_{};
		std::vector<LightManager::PunctualLightGPU> punctualLightsCpu_;
		std::vector<PunctualFrameBuffer> punctualFrames_;
	};
} // namespace Ken4lowEngine
