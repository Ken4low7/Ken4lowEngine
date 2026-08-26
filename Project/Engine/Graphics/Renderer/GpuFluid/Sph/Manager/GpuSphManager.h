#pragma once

#include "../Resource/GpuSphParticleBuffer.h"

#include <cstdint>

namespace Ken4lowEngine
{

/// W5 GPU SPH FoundationのRuntime所有者。W5.1ではParticle Bufferのみ管理する。
class GpuSphManager
{
public:
	static constexpr uint32_t kDefaultParticleCapacity = 65536;

	static GpuSphManager* GetInstance();

	bool Initialize(uint32_t particleCapacity = kDefaultParticleCapacity);
	void Finalize();

	[[nodiscard]] bool IsInitialized() const { return initialized_; }
	[[nodiscard]] GpuSphParticleBuffer& GetParticleBuffer() { return particleBuffer_; }
	[[nodiscard]] const GpuSphParticleBuffer& GetParticleBuffer() const { return particleBuffer_; }
	[[nodiscard]] GpuSphParticleBufferStats GetParticleBufferStats() const { return particleBuffer_.GetStats(); }

private:
	GpuSphManager() = default;
	~GpuSphManager() = default;
	GpuSphManager(const GpuSphManager&) = delete;
	GpuSphManager& operator=(const GpuSphManager&) = delete;

private:
	GpuSphParticleBuffer particleBuffer_{};
	bool initialized_ = false;
};

} // namespace Ken4lowEngine
