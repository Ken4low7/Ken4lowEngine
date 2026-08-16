#pragma once

#include <DX12Include.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Ken4lowEngine
{

enum class GpuParticleExecutionPassType : uint8_t
{
	Update = 0,
	Emit,
};

struct GpuParticleExecutionPass
{
	GpuParticleExecutionPassType type = GpuParticleExecutionPassType::Update;
	D3D12_GPU_VIRTUAL_ADDRESS emitterCbAddress = 0;
};

struct GpuParticleExecutionGraphStats
{
	uint32_t updatePassCount = 0;
	uint32_t emitPassCount = 0;
	uint32_t skippedUpdatePassCount = 0;

	[[nodiscard]] uint32_t GetPassCount() const
	{
		return updatePassCount + emitPassCount;
	}

	[[nodiscard]] uint32_t EstimateLegacyTransitionCount() const
	{
		return GetPassCount() * 2u;
	}

	[[nodiscard]] uint32_t EstimateBatchedTransitionCount() const
	{
		return GetPassCount() > 0u ? 2u : 0u;
	}

	[[nodiscard]] uint32_t EstimateUavBarrierCount() const
	{
		const uint32_t passCount = GetPassCount();
		return passCount > 0u ? passCount - 1u : 0u;
	}

	[[nodiscard]] uint32_t EstimatePipelineSwitchCount() const
	{
		return (updatePassCount > 0u ? 1u : 0u) + (emitPassCount > 0u ? 1u : 0u);
	}
};

class GpuParticleExecutionGraph
{
public:
	void Reset()
	{
		passes_.clear();
		stats_ = {};
	}

	void SetUpdateRequired(bool required)
	{
		if (!required)
		{
			++stats_.skippedUpdatePassCount;
			return;
		}

		// Update remains the first GPU pass so newly emitted particles keep the existing next-frame update contract.
		passes_.push_back({ GpuParticleExecutionPassType::Update, 0 });
		++stats_.updatePassCount;
	}

	void AddEmit(D3D12_GPU_VIRTUAL_ADDRESS emitterCbAddress)
	{
		if (emitterCbAddress == 0) return;
		passes_.push_back({ GpuParticleExecutionPassType::Emit, emitterCbAddress });
		++stats_.emitPassCount;
	}

	[[nodiscard]] bool HasWork() const { return !passes_.empty(); }
	[[nodiscard]] const std::vector<GpuParticleExecutionPass>& GetPasses() const { return passes_; }
	[[nodiscard]] const GpuParticleExecutionGraphStats& GetStats() const { return stats_; }

private:
	std::vector<GpuParticleExecutionPass> passes_;
	GpuParticleExecutionGraphStats stats_{};
};

} // namespace Ken4lowEngine
