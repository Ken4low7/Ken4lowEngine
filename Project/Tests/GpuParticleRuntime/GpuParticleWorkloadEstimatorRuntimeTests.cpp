#include "GpuParticleWorkloadEstimator.h"

#include <cassert>
#include <iostream>

using namespace Ken4lowEngine;

int main()
{
	// 131072 = 2^17なのでBitonic networkは17*18/2=153 passになる。
	static_assert(GpuParticleWorkloadEstimator::CountBitonicPasses(131072u) == 153u);
	static_assert(GpuParticleWorkloadEstimator::CountBitonicPasses(1000u) == 0u);

	const auto additiveOnly = GpuParticleWorkloadEstimator::Estimate(131072u, 8u, 0u);
	assert(additiveOnly.compactionParticleScans == 1048576u);
	assert(additiveOnly.compactionThreadGroups == 4096u);
	assert(additiveOnly.alphaSortDispatches == 0u);
	assert(additiveOnly.indirectDraws == 8u);

	const auto mixed = GpuParticleWorkloadEstimator::Estimate(131072u, 8u, 2u);
	assert(mixed.compactionParticleScans == 1048576u);
	assert(mixed.alphaSortDispatches == 306u);
	assert(mixed.indirectDraws == 8u);

	const auto clamped = GpuParticleWorkloadEstimator::Estimate(131072u, 2u, 10u);
	assert(clamped.alphaSortDispatches == 306u);

	std::cout << "GPU particle workload estimator runtime tests passed\n";
	return 0;
}
