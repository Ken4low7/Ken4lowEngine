#pragma once

#include "GpuFluidForwardRenderer.h"
#include "Engine/Graphics/Camera/Manager/CameraManager.h"
#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"

#include <deque>
#include <cstddef>
#include <cstdint>

namespace Ken4lowEngine
{

struct GpuFluidForwardPacketStats
{
	uint64_t queueFrameSerial = 0;
	size_t submittedPackets = 0;
};

/// Fluid DomainをTransparent Forward Queueへ接続する軽量Bridge。
class GpuFluidForwardRenderBridge
{
public:
	static GpuFluidForwardRenderBridge* GetInstance()
	{
		static GpuFluidForwardRenderBridge instance;
		return &instance;
	}

	bool Submit(
		ForwardRenderQueue& queue,
		GpuFluidForwardRenderer& renderer,
		GpuFluidGridResource& grid,
		const GpuFluidDomainMapping& domain,
		const GpuFluidRenderDesc& renderDesc)
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

		const float sortDepth = CalculateSortDepth(grid, domain);
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
					packet->renderDesc); // 実行時Active Viewで描き、Reflection Camera overrideも維持する。
			},
			MaterialBlendMode::Transparent,
			sortDepth);

		if (!queue.Submit(item))
		{
			packets_.pop_back();
			return false;
		}

		++lastPacketStats_.submittedPackets;
		return true;
	}

	[[nodiscard]] const GpuFluidForwardPacketStats& GetLastPacketStats() const
	{
		return lastPacketStats_;
	}

private:
	struct RenderPacket
	{
		GpuFluidForwardRenderer* renderer = nullptr;
		GpuFluidGridResource* grid = nullptr;
		GpuFluidDomainMapping domain{};
		GpuFluidRenderDesc renderDesc{};
	};

	static float CalculateSortDepth(
		const GpuFluidGridResource& grid,
		const GpuFluidDomainMapping& domain)
	{
		const GpuFluidGridDesc& gridDesc = grid.GetGridDesc();
		const Vector3 axisU = Vector3::NormalizeSafe(domain.axisU, { 1.0f, 0.0f, 0.0f });
		const Vector3 axisV = Vector3::NormalizeSafe(domain.axisV, { 0.0f, 1.0f, 0.0f });
		const Vector3 center = domain.origin +
			axisU * (static_cast<float>(gridDesc.width) * gridDesc.cellSize * 0.5f) +
			axisV * (static_cast<float>(gridDesc.height) * gridDesc.cellSize * 0.5f);

		CameraManager* cameraManager = CameraManager::GetInstance();
		if (cameraManager == nullptr)
		{
			return 0.0f;
		}

		return Vector3::Dot(
			center - cameraManager->GetActiveCameraPosition(),
			cameraManager->GetActiveCameraForward());
	}

	GpuFluidForwardRenderBridge() = default;

	std::deque<RenderPacket> packets_{}; // push_backで既存要素参照を維持し、Queue payloadをFrame中安定させる。
	GpuFluidForwardPacketStats lastPacketStats_{};
	uint64_t packetQueueSerial_ = 0;
};

} // namespace Ken4lowEngine
