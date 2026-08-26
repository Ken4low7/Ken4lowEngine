#include "GpuSphManager.h"

namespace Ken4lowEngine
{

GpuSphManager* GpuSphManager::GetInstance()
{
	static GpuSphManager instance;
	return &instance;
}

bool GpuSphManager::Initialize(uint32_t particleCapacity)
{
	Finalize();

	// W5.1では計算Passを起動せず、SPH Particle BufferのGPU契約だけを確立する。
	initialized_ = particleBuffer_.Initialize(particleCapacity);
	return initialized_;
}

void GpuSphManager::Finalize()
{
	particleBuffer_.Finalize();
	initialized_ = false;
}

} // namespace Ken4lowEngine
