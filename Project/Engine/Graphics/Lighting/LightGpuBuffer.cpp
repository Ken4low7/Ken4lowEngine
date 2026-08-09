#include "LightGpuBuffer.h"

#include "DirectXCommon.h"
#include <ResourceManager.h>
#include <SRVManager.h>

#include <algorithm>
#include <cstring>

namespace Ken4lowEngine
{
	void LightGpuBuffer::Initialize(DirectXCommon* dxCommon, const LightManager::LightingSettingsGPU& initialLightingSettings)
	{
		dxCommon_ = dxCommon;
		if (!dxCommon_)
		{
			return;
		}

		const uint32_t frameCount = (std::max)(1u, dxCommon_->GetCommandManager()->GetFrameResourceCount());
		lightInfoBuffers_.Initialize(dxCommon_->GetDevice(), frameCount);
		lightingSettingsBuffers_.Initialize(dxCommon_->GetDevice(), frameCount);

		lightInfoCpu_.lightCount = 0;
		lightInfoBuffers_.WriteAll(lightInfoCpu_);
		lightingSettingsBuffers_.WriteAll(initialLightingSettings);

		punctualFrames_.resize(frameCount);
		for (uint32_t frameIndex = 0; frameIndex < frameCount; ++frameIndex)
		{
			PunctualFrameBuffer& frame = punctualFrames_[frameIndex];
			frame.srvIndex = SRVManager::GetInstance()->Allocate();
			frame.srvAllocated = true;
			EnsurePunctualFrameCapacity(frameIndex, sizeof(LightManager::PunctualLightGPU), 1u);
		}
	}

	void LightGpuBuffer::Finalize()
	{
		for (PunctualFrameBuffer& frame : punctualFrames_)
		{
			if (frame.srvAllocated && frame.srvIndex != UINT32_MAX)
			{
				SRVManager::GetInstance()->Free(frame.srvIndex);
			}
			frame.resource.Reset();
			frame.bufferBytes = 0;
			frame.srvIndex = UINT32_MAX;
			frame.srvAllocated = false;
		}

		punctualFrames_.clear();
		punctualLightsCpu_.clear();
		lightInfoBuffers_.Finalize();
		lightingSettingsBuffers_.Finalize();
		dxCommon_ = nullptr;
	}

	void LightGpuBuffer::UpdatePunctualLights(
		const std::vector<LightManager::PunctualLightGPU>& punctualLights,
		const std::vector<LightManager::PunctualLightGPU>& lightComponentLights)
	{
		punctualLightsCpu_.clear();
		punctualLightsCpu_.reserve(punctualLights.size() + lightComponentLights.size());

		auto appendEnabledLights = [this](const std::vector<LightManager::PunctualLightGPU>& source)
			{
				for (const auto& light : source)
				{
					if (light.lightType == 0 || light.enabled == 0u)
					{
						continue;
					}

					LightManager::PunctualLightGPU normalized = light;
					if (normalized.lightType == 1 || normalized.lightType == 3 || normalized.lightType == 4 || normalized.lightType == 5)
					{
						normalized.direction = Vector3::Normalize(normalized.direction);
					}
					punctualLightsCpu_.push_back(normalized);
				}
			};

		appendEnabledLights(punctualLights);
		appendEnabledLights(lightComponentLights);

		lightInfoCpu_.lightCount = static_cast<uint32_t>(punctualLightsCpu_.size());
		const uint32_t frameIndex = GetCurrentFrameIndex();
		lightInfoBuffers_.WriteFrame(frameIndex, lightInfoCpu_);
		UploadPunctualLightsForFrame(frameIndex);
	}

	void LightGpuBuffer::BindPunctualLights(uint32_t rootIndexCB_b2, uint32_t rootIndexSRV_t2)
	{
		if (!dxCommon_ || punctualFrames_.empty())
		{
			return;
		}

		const uint32_t frameIndex = GetCurrentFrameIndex();
		lightInfoBuffers_.WriteFrame(frameIndex, lightInfoCpu_);
		UploadPunctualLightsForFrame(frameIndex);

		auto commandList = dxCommon_->GetCommandManager()->GetCommandList();
		const D3D12_GPU_VIRTUAL_ADDRESS lightInfoAddress = lightInfoBuffers_.GetGpuAddress(frameIndex);
		if (lightInfoAddress != 0)
		{
			commandList->SetGraphicsRootConstantBufferView(rootIndexCB_b2, lightInfoAddress);
		}

		const PunctualFrameBuffer& frame = punctualFrames_[frameIndex % punctualFrames_.size()];
		SRVManager::GetInstance()->PreDraw();
		SRVManager::GetInstance()->SetGraphicsRootDescriptorTable(rootIndexSRV_t2, frame.srvIndex);
	}

	void LightGpuBuffer::BindLightingSettings(uint32_t rootIndexCB_b5, const LightManager::LightingSettingsGPU& lightingSettings)
	{
		if (!dxCommon_)
		{
			return;
		}

		const uint32_t frameIndex = GetCurrentFrameIndex();
		lightingSettingsBuffers_.WriteFrame(frameIndex, lightingSettings); // 現在Frame専用CBへ書き込み、前FrameのGPU読み取りと競合させない。
		const D3D12_GPU_VIRTUAL_ADDRESS settingsAddress = lightingSettingsBuffers_.GetGpuAddress(frameIndex);
		if (settingsAddress != 0)
		{
			dxCommon_->GetCommandManager()->GetCommandList()->SetGraphicsRootConstantBufferView(rootIndexCB_b5, settingsAddress);
		}
	}

	uint32_t LightGpuBuffer::GetCurrentFrameIndex() const
	{
		return dxCommon_ && dxCommon_->GetCommandManager()
			? dxCommon_->GetCommandManager()->GetCurrentFrameIndex()
			: 0u;
	}

	void LightGpuBuffer::EnsurePunctualFrameCapacity(uint32_t frameIndex, uint32_t bytes, uint32_t elementCount)
	{
		if (!dxCommon_ || punctualFrames_.empty())
		{
			return;
		}

		PunctualFrameBuffer& frame = punctualFrames_[frameIndex % punctualFrames_.size()];
		const uint32_t safeBytes = (std::max)(bytes, static_cast<uint32_t>(sizeof(LightManager::PunctualLightGPU)));
		const uint32_t safeElementCount = (std::max)(1u, elementCount);
		if (!frame.resource || frame.bufferBytes < safeBytes)
		{
			frame.resource = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), safeBytes);
			frame.bufferBytes = safeBytes;
		}

		if (frame.resource && frame.srvAllocated)
		{
			SRVManager::GetInstance()->CreateSRVForStructureBuffer(
				frame.srvIndex,
				frame.resource.Get(),
				safeElementCount,
				sizeof(LightManager::PunctualLightGPU));
		}
	}

	void LightGpuBuffer::UploadPunctualLightsForFrame(uint32_t frameIndex)
	{
		if (!dxCommon_ || punctualFrames_.empty())
		{
			return;
		}

		const uint32_t elementCount = static_cast<uint32_t>(punctualLightsCpu_.size());
		const uint32_t safeElementCount = (std::max)(1u, elementCount);
		const uint32_t bytes = safeElementCount * static_cast<uint32_t>(sizeof(LightManager::PunctualLightGPU));
		EnsurePunctualFrameCapacity(frameIndex, bytes, safeElementCount);

		PunctualFrameBuffer& frame = punctualFrames_[frameIndex % punctualFrames_.size()];
		if (!frame.resource || elementCount == 0)
		{
			return;
		}

		void* mapped = nullptr;
		if (SUCCEEDED(frame.resource->Map(0, nullptr, &mapped)) && mapped)
		{
			std::memcpy(mapped, punctualLightsCpu_.data(), elementCount * sizeof(LightManager::PunctualLightGPU));
			frame.resource->Unmap(0, nullptr);
		}
	}
} // namespace Ken4lowEngine
