#pragma once

#include "Engine/Graphics/Renderer/Forward/ForwardRenderQueue.h"
#include "GpuParticleEmitter.h"
#include "GpuParticleManager.h"

#include <cstddef>
#include <cstdint>

namespace Ken4lowEngine
{
	struct GpuParticleForwardPacketStats
	{
		uint64_t queueFrameSerial = 0;
		size_t transparentPackets = 0;
		size_t additivePackets = 0;

		size_t GetTotalPackets() const { return transparentPackets + additivePackets; }
	};

	/// <summary>
	/// GPU Particle全体をCPU粒子単位へ分解せず、Blend分類ごとのSystem PacketとしてForward Queueへ接続します。
	/// Particle内部の可視抽出・Alpha Depth Sort・Indirect Drawは従来どおりGPU側で完結させます。
	/// </summary>
	class GpuParticleForwardRenderBridge
	{
	public:
		static GpuParticleForwardRenderBridge* GetInstance()
		{
			static GpuParticleForwardRenderBridge instance;
			return &instance;
		}

		bool Submit(ForwardRenderQueue& queue)
		{
			if (!queue.IsFrameActive()) return false;
			if (lastSubmittedQueueSerial_ == queue.GetFrameSerial()) return false;

			GpuParticleManager* manager = GpuParticleManager::GetInstance();
			if (!manager) return false;

			lastSubmittedQueueSerial_ = queue.GetFrameSerial();
			lastPacketStats_ = {};
			lastPacketStats_.queueFrameSerial = queue.GetFrameSerial();

			transparentPacket_.manager = manager;
			transparentPacket_.pass = GpuParticleForwardDrawPass::Transparent;
			additivePacket_.manager = manager;
			additivePacket_.pass = GpuParticleForwardDrawPass::Additive;

			bool submitted = false;
			if (HasActiveEmitters(*manager, GpuParticleForwardDrawPass::Transparent))
			{
				if (SubmitPacket(queue, transparentPacket_, MaterialBlendMode::Transparent))
				{
					++lastPacketStats_.transparentPackets;
					submitted = true;
				}
			}
			if (HasActiveEmitters(*manager, GpuParticleForwardDrawPass::Additive))
			{
				if (SubmitPacket(queue, additivePacket_, MaterialBlendMode::Additive))
				{
					++lastPacketStats_.additivePackets;
					submitted = true;
				}
			}
			return submitted;
		}

		size_t GetLastSubmittedPacketCount() const { return lastPacketStats_.GetTotalPackets(); }
		const GpuParticleForwardPacketStats& GetLastPacketStats() const { return lastPacketStats_; }

	private:
		struct RenderPacket
		{
			GpuParticleManager* manager = nullptr;
			GpuParticleForwardDrawPass pass = GpuParticleForwardDrawPass::All;
		};

		class ScopedDrawPass
		{
		public:
			explicit ScopedDrawPass(GpuParticleForwardDrawPass pass)
				: previous_(GpuParticleEmitter::GetForwardDrawPass())
			{
				GpuParticleEmitter::SetForwardDrawPass(pass);
			}

			~ScopedDrawPass()
			{
				GpuParticleEmitter::SetForwardDrawPass(previous_);
			}

		private:
			GpuParticleForwardDrawPass previous_ = GpuParticleForwardDrawPass::All;
		};

		static bool HasActiveEmitters(GpuParticleManager& manager, GpuParticleForwardDrawPass pass)
		{
			const ScopedDrawPass scope(pass);
			return manager.GetActiveEmitterCount() > 0;
		}

		bool SubmitPacket(ForwardRenderQueue& queue, RenderPacket& packet, MaterialBlendMode blendMode)
		{
			ForwardRenderItem item = MakeForwardRenderItem(
				&packet,
				[](void* payload)
				{
					auto* renderPacket = static_cast<RenderPacket*>(payload);
					if (!renderPacket || !renderPacket->manager) return;
					const ScopedDrawPass scope(renderPacket->pass);
					renderPacket->manager->Draw(); // CPU QueueはSystem Packetだけを順序付けし、粒子選別とIndirect DrawはGPUへ残す。
				},
				blendMode,
				0.0f);

			// 1 Render Groupが複数Emitterを跨ぐためCPU側に単一の正確なDepthは存在しない。
			// QueueではBlend大分類とstable orderを保証し、Alpha粒子の厳密な深度順は既存GPU Bitonic Sortへ委譲する。
			return queue.Submit(item);
		}

		GpuParticleForwardRenderBridge() = default;

		RenderPacket transparentPacket_{};
		RenderPacket additivePacket_{};
		GpuParticleForwardPacketStats lastPacketStats_{};
		uint64_t lastSubmittedQueueSerial_ = 0;
	};
} // namespace Ken4lowEngine
