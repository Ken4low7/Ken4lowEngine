#pragma once

#include "GpuVolumetricFluidRaymarchRenderer.h"
#include "Engine/Graphics/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"

#include <cstddef>
#include <cstdint>
#include <deque>

namespace Ken4lowEngine
{

struct GpuVolumetricFluidForwardPacketStats
{
	uint64_t queueFrameSerial = 0;
	size_t submittedPackets = 0;
};

/// Volume RaymarchをTransparent Forward Queueへ接続するBridge。
class GpuVolumetricFluidForwardRenderBridge
{
public:
	static GpuVolumetricFluidForwardRenderBridge* GetInstance()
	{
		static GpuVolumetricFluidForwardRenderBridge instance;
		return &instance;
	}

	bool Submit(
		ForwardRenderQueue& queue,
		GpuVolumetricFluidRaymarchRenderer& renderer,
		GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidDomainMapping& domain,
		const GpuVolumetricFluidRenderDesc& renderDesc)
	{
		if (!queue.IsFrameActive() || !renderer.IsInitialized() ||
			!grid.IsInitialized() || !domain.IsValid() || !renderDesc.IsValid())
		{
			return false;
		}

		if (packetQueueSerial_ != queue.GetFrameSerial())
		{
			packets_.clear();
			packetQueueSerial_ = queue.GetFrameSerial();
			lastPacketStats_ = {};
			lastPacketStats_.queueFrameSerial = packetQueueSerial_;
		}

		RenderPacket packet{};
		packet.renderer = &renderer;
		packet.grid = &grid;
		packet.domain = domain;
		packet.renderDesc = renderDesc;
		packets_.push_back(packet);

		ForwardRenderItem item = MakeForwardRenderItem(
			&packets_.back(),
			[](void* payload)
			{
				auto* packet = static_cast<RenderPacket*>(payload);
				if (!packet || !packet->renderer || !packet->grid)
				{
					return;
				}

				packet->renderer->Draw(
					*packet->grid,
					packet->domain,
					packet->renderDesc); // Draw時Active Viewを解決してReflection Camera overrideを維持する。
			},
			MaterialBlendMode::Transparent,
			CalculateSortDepth(grid, domain),
			true); // Scene Depthでray終端するVolumeだけForward QueueへDepth SRV化を明示要求する。

		if (!queue.Submit(item))
		{
			packets_.pop_back();
			return false;
		}

		++lastPacketStats_.submittedPackets;
		return true;
	}

	[[nodiscard]] const GpuVolumetricFluidForwardPacketStats& GetLastPacketStats() const
	{
		return lastPacketStats_;
	}

private:
	struct RenderPacket
	{
		GpuVolumetricFluidRaymarchRenderer* renderer = nullptr;
		GpuVolumetricFluidGridResource* grid = nullptr;
		GpuVolumetricFluidDomainMapping domain{};
		GpuVolumetricFluidRenderDesc renderDesc{};
	};

	static float CalculateSortDepth(
		const GpuVolumetricFluidGridResource& grid,
		const GpuVolumetricFluidDomainMapping& domain)
	{
		const GpuVolumetricFluidGridDesc& gridDesc = grid.GetGridDesc();
		const Vector3 axisU = Vector3::NormalizeSafe(domain.axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 axisV = Vector3::NormalizeSafe(domain.axisV, { 0.0f, 1.0f, 0.0f });
		const Vector3 axisW = Vector3::NormalizeSafe(domain.axisW, { 0.0f, 0.0f, 1.0f });
		const Vector3 center = domain.origin +
			axisU * (static_cast<float>(gridDesc.width) * gridDesc.cellSize * 0.5f) +
			axisV * (static_cast<float>(gridDesc.height) * gridDesc.cellSize * 0.5f) +
			axisW * (static_cast<float>(gridDesc.depth) * gridDesc.cellSize * 0.5f);

		CameraManager* cameraManager = CameraManager::GetInstance();
		if (cameraManager == nullptr)
		{
			return 0.0f;
		}

		return Vector3::Dot(
			center - cameraManager->GetActiveCameraPosition(),
			cameraManager->GetActiveCameraForward());
	}

	GpuVolumetricFluidForwardRenderBridge() = default;

	std::deque<RenderPacket> packets_{}; // Queue payloadの参照をForward Frame中安定させる。
	GpuVolumetricFluidForwardPacketStats lastPacketStats_{};
	uint64_t packetQueueSerial_ = 0;
};

} // namespace Ken4lowEngine