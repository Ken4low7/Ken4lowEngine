#pragma once

#include <cstdint>

namespace Ken4lowEngine
{
	struct GpuParticleWorkloadEstimate
	{
		uint64_t compactionParticleScans = 0;
		uint64_t compactionThreadGroups = 0;
		uint64_t alphaSortDispatches = 0;
		uint64_t indirectDraws = 0;
	};

	/// GPU readback無しで、RenderGroup数からPhase14の理論Dispatch workloadを事前評価する軽量Estimatorです。
	class GpuParticleWorkloadEstimator
	{
	public:
		static constexpr uint32_t kCompactionThreadCount = 256;

		static constexpr bool IsPowerOfTwo(uint32_t value)
		{
			return value != 0u && (value & (value - 1u)) == 0u;
		}

		static constexpr uint32_t CountBitonicPasses(uint32_t itemCount)
		{
			if (!IsPowerOfTwo(itemCount)) return 0u;

			uint32_t levels = 0u;
			for (uint32_t value = itemCount; value > 1u; value >>= 1u)
			{
				++levels;
			}
			return (levels * (levels + 1u)) / 2u;
		}

		static constexpr GpuParticleWorkloadEstimate Estimate(
			uint32_t maxParticles,
			uint32_t renderGroupCount,
			uint32_t alphaRenderGroupCount)
		{
			GpuParticleWorkloadEstimate result{};
			if (maxParticles == 0u || renderGroupCount == 0u) return result;

			const uint32_t clampedAlphaGroups =
				alphaRenderGroupCount < renderGroupCount ? alphaRenderGroupCount : renderGroupCount;
			const uint64_t threadGroupsPerCompaction =
				(static_cast<uint64_t>(maxParticles) + kCompactionThreadCount - 1u) / kCompactionThreadCount;

			result.compactionParticleScans = static_cast<uint64_t>(maxParticles) * renderGroupCount;
			result.compactionThreadGroups = threadGroupsPerCompaction * renderGroupCount;
			result.alphaSortDispatches =
				static_cast<uint64_t>(CountBitonicPasses(maxParticles)) * clampedAlphaGroups;
			result.indirectDraws = renderGroupCount;
			return result;
		}
	};

	static_assert(GpuParticleWorkloadEstimator::CountBitonicPasses(131072u) == 153u); // 現行poolのSort cost変更を意図せず見落とさない。
} // namespace Ken4lowEngine
