#pragma once

#include "Engine/Graphics/Material/Material.h"

#include <cstdint>
#include <unordered_map>

namespace Ken4lowEngine
{
	class Actor;

	struct PlanarReflectionCaptureStats
	{
		uint32_t drawableCount = 0;
		uint32_t opaqueCount = 0;
		uint32_t maskedCount = 0;
		uint32_t transparentCount = 0;
		uint32_t additiveCount = 0;
	};

	/// <summary>Editor診断用に、鏡Actorごとの直近Capture候補数を保持します。</summary>
	class PlanarReflectionCaptureDiagnostics
	{
	public:
		static PlanarReflectionCaptureDiagnostics* GetInstance()
		{
			static PlanarReflectionCaptureDiagnostics instance;
			return &instance;
		}

		void Record(const Actor* receiverActor, const PlanarReflectionCaptureStats& stats)
		{
			if (!receiverActor) return;
			statsByReceiver_[receiverActor] = stats; // Capture対象列挙の結果だけを保持し、描画Resourceの所有権は持たない。
		}

		PlanarReflectionCaptureStats Get(const Actor* receiverActor) const
		{
			if (!receiverActor) return {};
			const auto found = statsByReceiver_.find(receiverActor);
			return found != statsByReceiver_.end() ? found->second : PlanarReflectionCaptureStats{};
		}

	private:
		PlanarReflectionCaptureDiagnostics() = default;
		std::unordered_map<const Actor*, PlanarReflectionCaptureStats> statsByReceiver_{};
	};
} // namespace Ken4lowEngine
