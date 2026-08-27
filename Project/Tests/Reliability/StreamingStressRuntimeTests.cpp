#include "WorldPartitionGrid.h"

#include <cassert>
#include <cstdint>
#include <iostream>

using namespace Ken4lowEngine;

int main()
{
	constexpr float cellSize = 128.0f;
	uint64_t loadCount = 0;
	uint64_t retainCount = 0;
	uint64_t unloadCount = 0;

	for (int iteration = 0; iteration < 200000; ++iteration)
	{
		const int routeX = ((iteration / 97) % 17) - 8;
		const int routeZ = ((iteration / 53) % 19) - 9;
		const WorldPartitionCell source = WorldPartitionGrid::WorldToCell(
			static_cast<float>(routeX) * cellSize + 0.25f,
			static_cast<float>(routeZ) * cellSize + 0.75f,
			cellSize);

		for (int offsetX = -4; offsetX <= 4; ++offsetX)
		{
			for (int offsetZ = -4; offsetZ <= 4; ++offsetZ)
			{
				const WorldPartitionCell target{ source.x + offsetX, source.z + offsetZ };
				switch (WorldPartitionGrid::Evaluate(source, target, false, 1, 2))
				{
				case WorldPartitionStreamingDecision::Load:
					++loadCount;
					break;
				case WorldPartitionStreamingDecision::Retain:
					++retainCount;
					break;
				case WorldPartitionStreamingDecision::Unload:
					++unloadCount;
					break;
				case WorldPartitionStreamingDecision::AlwaysLoaded:
					assert(false);
					break;
				}
			}
		}
	}

	// Stress coverage must exercise load, hysteresis retain and unload paths many times, including negative cells.
	assert(loadCount > 1000000);
	assert(retainCount > 1000000);
	assert(unloadCount > 1000000);
	assert(WorldPartitionGrid::WorldToCell(-0.1f, -128.1f, cellSize) == WorldPartitionCell({ -1, -2 }));
	assert(WorldPartitionGrid::Evaluate({ 0, 0 }, { 99, 99 }, true, 1, 2) ==
		WorldPartitionStreamingDecision::AlwaysLoaded);

	std::cout << "Streaming stress runtime tests passed\n";
	return 0;
}
